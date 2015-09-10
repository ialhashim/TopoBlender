#include "Explore.h"
#include "ui_Explore.h"

#include "Document.h"
#include "Camera.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"

#include "Thumbnail.h"

#include "SimpleMatrix.h"

#include "Model.h"

#include "voronoi.hpp"
using namespace cinekine;

#include "DivergingColorMaps.hpp"

Explore::Explore(Document *document, const QRectF &bounds) : Tool(document)
{
    setBounds(bounds);
    setObjectName("explore");
}

void Explore::init()
{
	setProperty("hasBackground", true);
	setProperty("backgroundColor", QColor(0,0,0,50));

    // Add widget
    auto toolsWidgetContainer = new QWidget();
    widget = new Ui::ExploreWidget();
    widget->setupUi(toolsWidgetContainer);
    widgetProxy = new QGraphicsProxyWidget(this);
    widgetProxy->setWidget(toolsWidgetContainer);

    // Place at bottom left corner
    auto delta = widgetProxy->sceneBoundingRect().bottomLeft() -
    scene()->views().front()->rect().bottomLeft();
    widgetProxy->moveBy(-delta.x(), -delta.y());

    // Fill categories box
    {
        for(auto cat : document->categories.keys()){
            widget->categoriesBox->insertItem(widget->categoriesBox->count(), cat);
        }

        int idx = widget->categoriesBox->findText(document->categoryOf(document->firstModelName()));
        widget->categoriesBox->setCurrentIndex(idx);
    }

    // Connect UI with actions
    {
        connect(widget->categoriesBox, &QComboBox::currentTextChanged, [&](QString text){
            document->currentCategory = text;
        });

        connect(widget->analyzeButton, &QPushButton::pressed, [&](){
            document->computePairwise(widget->categoriesBox->currentText());
        });

        connect(document, &Document::categoryPairwiseDone, [=](){
            // Clean up earlier results
            for(auto i : this->childItems()) if(i != widgetProxy) scene()->removeItem(i);

            auto catModels = document->categories[ document->currentCategory ].toStringList();

            smat::Matrix<double> D(catModels.size(), catModels.size(), 0);

            // Range
            double min_val = 1.0, max_val = 0.0;

            for(int i = 0; i < catModels.size(); i++)
            {
                for(int j = i+1; j < catModels.size(); j++)
                {
                    auto s = catModels[i], t = catModels[j];

                    double dist = std::max(document->datasetMatching[s][t]["min_cost"].toDouble(),
                                           document->datasetMatching[t][s]["min_cost"].toDouble());

                    D.set(i,j,dist);
                    D.set(j,i,dist);

                    // Non-zero
                    if(dist != 0){
                        min_val = std::min(min_val, dist);
                        max_val = std::max(max_val, dist);
                    }
                }
            }

            // Default view angle
            {
                // Camera target and initial position
                auto camera = new Eigen::Camera();
                auto frame = camera->frame();
                frame.position = Eigen::Vector3f(-1, 0, 0.5);
                camera->setTarget(Eigen::Vector3f(0, 0, 0.5));
                camera->setFrame(frame);

                int deltaZoom = document->extent().length() * 1.0;

                // Default view angle
                double theta1 = acos(-1) * 0.75;
                double theta2 = acos(-1) * 0.10;
                camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
                camera->zoom(-(4+deltaZoom));
                camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));
                auto cp = camera->position();
                cameraPos = QVector3D(cp[0], cp[1], cp[2]);

                // Camera settings
                camera->setViewport(128, 128);
                Eigen::Matrix4f p = camera->projectionMatrix();
                Eigen::Matrix4f v = camera->viewMatrix().matrix();
                p.transposeInPlace();
                v.transposeInPlace();
                cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());
            }

            // From surface mesh to basic mesh
            auto toBasicMesh = [&](opengp::SurfaceMesh::SurfaceMeshModel * m, QColor color){
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
            };

            // MDS
            QSharedPointer< smat::Matrix<double> > X;

            if(widget->vizOption->currentIndex() == 0) X = QSharedPointer< smat::Matrix<double> >(smat::MDS_SMACOF(&D, NULL, 2, 30));
            if(widget->vizOption->currentIndex() == 1) X = QSharedPointer< smat::Matrix<double> >(smat::MDS_UCF(&D, NULL, 2, 30));

            /// Plot:
            // Compute spatial range:
            QPolygonF allPoints;
            for(int i = 0; i < catModels.size(); i++)
            {
                allPoints << QPointF(X->get(i,0), X->get(i,1));
            }
            auto allPointsRect = allPoints.boundingRect();

            // Plot points
            double dx = bounds.width() * 0.2;
            double dy = bounds.height() * 0.2;
            QRectF r(0, 0, bounds.width() - dx, bounds.height() - dy);

			QRectF thumbRect(0, 0, 150, 150);

            QMap<QString, Thumbnail*> thumbs;

			voronoi::Sites sites;

			for (int i = 0; i < catModels.size(); i++)
			{
				auto s = catModels[i];

				auto posRelative = QPointF((X->get(i, 0) - allPointsRect.left()) / allPointsRect.width(),
					(X->get(i, 1) - allPointsRect.top()) / allPointsRect.height());
				auto pos = QPointF((posRelative.x() * r.width()) + (dx / 2.0), (posRelative.y() * r.height()) + (dy / 2.0));

				sites.push_back(voronoi::Vertex(pos.x(), pos.y()));
			}

			//voronoi::Graph graph = voronoi::build(voronoi::lloyd_relax(sites, bounds.width(), bounds.height(), 5), bounds.width(), bounds.height());
			voronoi::Graph graph = voronoi::build(std::move(sites), bounds.width(), bounds.height());
			
			for (int i = 0; i < catModels.size(); i++)
			{
				auto s = catModels[i];

				auto site = graph.sites().at(i);
				QPointF pos (site.x, site.y);

                auto t = new Thumbnail(this, thumbRect);
                //t->setCaption(s);
				t->setCaption("");
                t->setMesh();
                t->setCamera(cameraPos, cameraMatrix);

				t->setProperty("isNoBackground", true);
				t->setProperty("isNoBorder", true);

                // Add parts of target shape
                auto m = document->cacheModel(s);
                for (auto n : m->nodes){
                    t->addAuxMesh(toBasicMesh(m->getMesh(n->id), n->vis_property["color"].value<QColor>()));
                }

                t->setPos(pos - thumbRect.center());

                thumbs[s] = t;
            }

            /*
               Return a RGB color value given a scalar v in the range [vmin,vmax]
               In this case each color component ranges from 0 (no contribution) to
               1 (fully saturated), modifications for other ranges is trivial.
               The color is clipped at the end of the scales if v is outside
               the range [vmin,vmax] - from StackOverflow/7706339
            */
            auto qtJetColor = [&](double v, double vmin,double vmax){
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
            };

			// Visualize Edges
			auto colorMapTable = makeColorMap();
            for(int i = 0; i < catModels.size(); i++)
            {
				double lowestCost = D.maxiumumColumn(i);

                for(int j = i+1; j < catModels.size(); j++)
                {
                    auto s = catModels[i], t = catModels[j];

                    auto t1 = thumbs[s], t2 = thumbs[t];

                    QLineF linef(this->mapFromItem(t1, t1->boundingRect().center()),
                                 this->mapFromItem(t2, t2->boundingRect().center()));

                    double similarity = 1.0 - ((D.get(i,j) - min_val) / (max_val - min_val));
					similarity = pow(similarity, 3);

					// Check if edge is in triangulation
					if (similarity >= 0.25)
                    {
						auto site = graph.sites().at(i);
						auto cell = graph.cells()[site.cell];
						for (auto he : cell.halfEdges){
							if (he.edge < 0) continue;
							auto edge = graph.edges()[he.edge];

							bool test1 = edge.leftSite == i && edge.rightSite == j;
							bool test2 = edge.rightSite == i && edge.leftSite == j;
							bool test3 = lowestCost == D.get(i, j);

							if (test1 || test2 || test3)
							{
								//auto c = getColorFromMap(similarity, colorMapTable);
								//QColor color(c[0], c[1], c[2]);
								//QColor color = starlab::qtColdColor(similarity);
								QColor color = starlab::qtJetColor(similarity);
								color.setAlphaF(0.5 * similarity);

								auto line = scene()->addLine(linef, QPen(color, similarity * 3));

								line->setParentItem(this);
								line->setFlag(QGraphicsItem::ItemNegativeZStacksBehindParent);
								line->setZValue(-1);
							}
						}
                    }
                }
            }
        });
    }
}
