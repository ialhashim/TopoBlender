#include "ExploreProcess.h"

#include "Document.h"
#include "Model.h"
#include "Camera.h"

#include "DivergingColorMaps.hpp"

#include "SimpleMatrix.h"

#include "voronoi.hpp"
using namespace cinekine;

// Blending
#include "GraphCorresponder.h"
#include "TopoBlender.h"
#include "Scheduler.h"
#include "SynthesisManager.h"

QPair<QVector3D,QMatrix4x4> ExploreProcess::defaultCamera(double zoomFactor)
{
    // Camera target and initial position
    auto camera = new Eigen::Camera();
    auto frame = camera->frame();
    frame.position = Eigen::Vector3f(-1, 0, 0.5);
    camera->setTarget(Eigen::Vector3f(0, 0, 0.5));
    camera->setFrame(frame);

    int deltaZoom = zoomFactor * 1.0;

    // Default view angle
    double theta1 = acos(-1) * 0.75;
    double theta2 = acos(-1) * 0.10;
    camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
    camera->zoom(-(4+deltaZoom));
    camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));
    auto cp = camera->position();
    auto cameraPos = QVector3D(cp[0], cp[1], cp[2]);

    // Camera settings
    camera->setViewport(128, 128);
    Eigen::Matrix4f p = camera->projectionMatrix();
    Eigen::Matrix4f v = camera->viewMatrix().matrix();
    p.transposeInPlace();
    v.transposeInPlace();
    auto cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());

    return qMakePair(cameraPos,cameraMatrix);
}

// From surface mesh to basic mesh
Thumbnail::QBasicMesh ExploreProcess::toBasicMesh (opengp::SurfaceMesh::SurfaceMeshModel * m, QColor color)
{
    Thumbnail::QBasicMesh mesh;
    m->update_face_normals();
    for (auto f : m->faces()){
        QVector<QVector3D> fp, fn;
        for (auto vf : m->vertices(f)){
            auto p = m->vertex_coordinates()[vf];
            auto n = m->face_normals()[f];
            fp << QVector3D(p[0], p[1], p[2]);
            fn << QVector3D(n[0], n[1], n[2]);
        }
        mesh.addTri(fp[0], fp[1], fp[2], fn[0], fn[1], fn[2]);
    }

    mesh.color = color;
    return mesh;
}

Thumbnail * ExploreProcess::makeThumbnail(QGraphicsItem * parent, Document * document, QString s, QPointF pos)
{
    QRectF thumbRect(0, 0, 150, 150);

    auto defaultCam = ExploreProcess::defaultCamera(document->extent().length());
    auto cameraPos = defaultCam.first;
    auto cameraMatrix = defaultCam.second;

    auto t = new Thumbnail(parent, thumbRect);
    //t->setCaption(s);
    t->setCaption("");
    t->setMesh();
    t->setCamera(cameraPos, cameraMatrix);

    QVariantMap data;
    data["shape"] = s;
    t->setData(data);

    t->setProperty("isNoBackground", true);
    t->setProperty("isNoBorder", true);

    // Add parts of target shape
    auto m = document->cacheModel(s);
    for (auto n : m->nodes){
        t->addAuxMesh(toBasicMesh(m->getMesh(n->id), n->vis_property["color"].value<QColor>()));
    }

    t->setPos(pos - thumbRect.center());

    return t;
}

/*
   Return a RGB color value given a scalar v in the range [vmin,vmax]
   In this case each color component ranges from 0 (no contribution) to
   1 (fully saturated), modifications for other ranges is trivial.
   The color is clipped at the end of the scales if v is outside
   the range [vmin,vmax] - from StackOverflow/7706339
*/
QColor ExploreProcess::qtJetColor(double v, double vmin, double vmax)
{
    double dv;
    if (v < vmin) v = vmin;
    if (v > vmax) v = vmax;
    dv = vmax - vmin;
    double r = 1, g = 1, b = 1;
    if (v < (vmin + 0.25 * dv)) {
        r = 0;
        g = 4 * (v - vmin) / dv;
    } else if (v < (vmin + 0.5 * dv)) {
        r = 0;
        b = 1 + 4 * (vmin + 0.25 * dv - v) / dv;
    } else if (v < (vmin + 0.75 * dv)) {
        r = 4 * (v - vmin - 0.5 * dv) / dv;
        b = 0;
    } else {
        g = 1 + 4 * (vmin + 0.75 * dv - v) / dv;
        b = 0;
    }
    return QColor::fromRgbF(qMax(0.0, qMin(r,1.0)), qMax(0.0, qMin(g,1.0)), qMax(0.0, qMin(b,1.0)));
}

QPolygonF ExploreProcess::embed(QMap<int, QMap<int, double> > distMatrix, int embedOption)
{
    QSharedPointer< smat::Matrix<double> > X;
    smat::Matrix<double> D(distMatrix.size(), distMatrix.size(), 0);

    for(auto i : distMatrix.keys())
    {
        for(auto j : distMatrix[i].keys())
        {
            double dist = distMatrix[i][j];
            D.set(i,j,dist);
            D.set(j,i,dist);
        }
    }

    // MDS
    if(embedOption == 0) X = QSharedPointer< smat::Matrix<double> >(smat::MDS_SMACOF(&D, NULL, 2, 200));
    if(embedOption == 1) X = QSharedPointer< smat::Matrix<double> >(smat::MDS_UCF(&D, NULL, 2, 200));

    QPolygonF allPoints;
    for(int i = 0; i < D.rows(); i++)
        allPoints << QPointF(X->get(i,0), X->get(i,1));
    return allPoints;
}

ExploreProcess::BasicVoronoiGraph ExploreProcess::buildGraph(QPolygonF points, double boundX, double boundY)
{
    voronoi::Sites sites;
    for(auto p : points) sites.push_back(voronoi::Vertex(p.x(), p.y()));

    //auto graph = voronoi::Graph graph = voronoi::build(voronoi::lloyd_relax(sites, bounds.width(),
    //   bounds.height(), 5), bounds.width(), bounds.height());
    auto graph =  voronoi::build(std::move(sites), boundX, boundY);

    BasicVoronoiGraph g;
	auto cells = graph.cells();
	g.cells.resize(cells.size());

	for (int i = 0; i < (int)cells.size(); i++)
    {
		auto c = cells[i];

        std::vector<BasicVoronoiGraph::Cell::Edge> edges;
        for(voronoi::HalfEdge he : c.halfEdges)
        {
            voronoi::Edge e = graph.edges()[he.edge];
            edges.push_back( qMakePair(e.leftSite, e.rightSite) );
        }

        BasicVoronoiGraph::Cell cell;
        cell.edges = edges;
        cell.pos = QPointF(graph.sites()[c.site].x,graph.sites()[c.site].y);

        g.cells[c.site] = cell;
    }
    return g;
}

void ExploreProcess::BlendPath::prepare(Document *document)
{
    if(isReady) return;

    auto cacheSource = document->cacheModel(source);
    auto cacheTarget = document->cacheModel(target);
    if(cacheSource == nullptr || cacheTarget == nullptr) return;

    auto source = QSharedPointer<Structure::Graph>(cacheSource->cloneAsShapeGraph());
    auto target = QSharedPointer<Structure::Graph>(cacheTarget->cloneAsShapeGraph());

    gcorr = QSharedPointer<GraphCorresponder>(new GraphCorresponder(source.data(), target.data()));

    // Apply computed correspondence
    if (false) // enable/disable auto correspondence
    {
        for (auto n : source->nodes)
        {
            QString sid = n->id;

            if (document->datasetCorr[this->source][sid][this->target].empty())
            {
                gcorr->setNonCorresSource(sid);
            }
            else
            {
                auto tid = document->datasetCorr[this->source][sid][this->target].front();
                gcorr->addLandmarks(QVector<QString>() << sid, QVector<QString>() << tid);
            }
        }
    }

    gcorr->computeCorrespondences();

    // Schedule blending sequence
    scheduler = QSharedPointer<Scheduler>(new Scheduler);
    blender = QSharedPointer<TopoBlender>(new TopoBlender(gcorr.data(), scheduler.data()));

    // Sample geometries
    int numSamples = 100;
    int reconLevel = 4;

    int LOD = 0;
    switch (LOD){
        case 0: numSamples = 10; reconLevel = 4; break;
        case 1: numSamples = 100; reconLevel = 4; break;
        case 2: numSamples = 1000; reconLevel = 5; break;
        case 3: numSamples = 10000; reconLevel = 7; break;
    }

    synthManager = QSharedPointer<SynthesisManager>(new SynthesisManager(gcorr.data(), scheduler.data(), blender.data(), numSamples));
    synthManager->genSynData();
    synthManager->property["reconLevel"].setValue(reconLevel);

    // Compute blending
    scheduler->timeStep = 1.0 / 100.0;
    scheduler->defaultSchedule();
    scheduler->executeAll();

    isReady = true;
}

QVector<Thumbnail::QBasicMesh> ExploreProcess::BlendPath::blend()
{
    QVector<Thumbnail::QBasicMesh> parts;
    if(!isReady) return parts;

    return parts;
}
