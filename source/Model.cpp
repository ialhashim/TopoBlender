#include "Model.h"
#include "Viewer.h"
#include "GeometryHelper.h"

using namespace opengp;

#include "ModelMesher.h"

Q_DECLARE_METATYPE(Array1D_Vector3);
Q_DECLARE_METATYPE(Vector3);

Model::Model(QObject *parent) : QObject(parent), Structure::ShapeGraph(""), activeNode(nullptr)
{

}

void Model::createCurveFromPoints(QVector<QVector3D> & points)
{
    if(points.size() < 2) return;

    // Nicer curve
    points = GeometryHelper::smooth(GeometryHelper::uniformResampleCount(points, 20), 5);

    // Create curve
    Array1D_Vector3 controlPoints;
    for(auto p : points) controlPoints.push_back(Vector3(p[0],p[1],p[2]));

    auto newCurveGeometry = NURBS::NURBSCurved::createCurveFromPoints(controlPoints);
    QString newCurveID = QString("curveNode%1").arg((nodes.empty()? -1 : nodes.back()->property["index"].toInt())+1);
    auto newCurve = new Structure::Curve(newCurveGeometry, newCurveID);

    // Add curve
    activeNode = this->addNode(newCurve);

    generateSurface();
}

void Model::createSheetFromPoints(QVector<QVector3D> &points)
{
    if (points.size() < 2) return;

    // Convert corner points to Vector3
    Array1D_Vector3 cp;
    for(auto p : points) cp.push_back(Vector3(p[0],p[1],p[2]));

    // midpoint of diagonal
    Vector3 center = 0.5 * (cp[1] + cp[2]);

    // Extent
    Vector3 du = cp[0] - cp[2], dv = cp[0] - cp[1];
    double u = du.norm(), v = dv.norm();

    // Create sheet
    auto newSheetGeometry = NURBS::NURBSRectangled::createSheet(u, v, center, du.normalized(), dv.normalized());
    QString newSheetID = QString("sheetNode%1").arg((nodes.empty()? -1 : nodes.back()->property["index"].toInt())+1);
    auto newSheet = new Structure::Sheet(newSheetGeometry, newSheetID);

    // Add sheet
    activeNode = this->addNode(newSheet);

    generateSurface();
}

QVector<Structure::Node *> Model::makeDuplicates(Structure::Node *n, QString duplicationOp)
{
    QVector<Structure::Node *> result;

    QStringList params = duplicationOp.split(",", QString::SkipEmptyParts);
    if(params.empty() || n == nullptr) return result;

    QString op = params.takeFirst();

    // Translational symmetry
    if(op == "dupT")
    {
        int count = params[0].toInt();

        double x = params[1].toDouble();
        double y = params[2].toDouble();
        double z = params[3].toDouble();

        Vector3 d = Vector3(x,y,z) / count;

        for(int i = 1; i < count; i++)
        {
            auto cloneNode = n->clone();

            auto delta = Vector3(d) * i;

            // Apply duplication operation
            {
                // Apply translation to node geometry
                auto nodeGeometry = cloneNode->controlPoints();
                for(auto & p : nodeGeometry) p += delta;
                cloneNode->setControlPoints(nodeGeometry);

                // Clone mesh and apply translation
                auto mesh = getMesh(n->id);
                auto cloneMesh = mesh->clone();
                for(auto v : cloneMesh->vertices()) cloneMesh->vertex_coordinates()[v] += delta;
                cloneMesh->update_face_normals();
                cloneMesh->update_vertex_normals();
                cloneMesh->updateBoundingBox();
                cloneNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(cloneMesh));
            }

            result.push_back(cloneNode);
        }
    }

    // Reflectional symmetry
    if(op == "dupRef")
    {
        auto x = params[0].toInt();
        auto y = params[1].toInt();
        auto z = params[2].toInt();
        double offset = params[3].toDouble();
        Vector3 planeNormal(x?1:0, y?1:0, z?1:0);

        auto cloneNode = n->clone();

        // Apply duplication operation
        {
            auto reflectPoint = [&](Vector3 V, Vector3 N, double offset){
                return (-2*(V).dot(N)*N + V) + offset * N;
            };

            // Apply reflection to node
            auto nodeGeometry = cloneNode->controlPoints();
            for(auto & p : nodeGeometry) p = reflectPoint(p, planeNormal, offset);
            cloneNode->setControlPoints(nodeGeometry);

            // Clone a reflected mesh
            auto mesh = getMesh(n->id);
            auto cloneMesh = new SurfaceMeshModel(cloneNode->id + ".obj", cloneNode->id);
            for(auto v : mesh->vertices()){
                cloneMesh->add_vertex(reflectPoint(mesh->vertex_coordinates()[v], planeNormal, offset));
            }
            for(auto f : mesh->faces()){
                std::vector<SurfaceMeshModel::Vertex> verts;
                for(auto v : mesh->vertices(f)) verts.push_back(v);
                std::reverse(verts.begin(), verts.end());
                cloneMesh->add_face(verts);
            }
            cloneMesh->update_face_normals();
            cloneMesh->update_vertex_normals();
            cloneMesh->updateBoundingBox();
            cloneNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(cloneMesh));
        }

        result.push_back(cloneNode);
    }

    // Rotational symmetry
    if(op == "dupRot")
    {
        int count = params[0].toInt();

        int x = params[1].toInt();
        int y = params[2].toInt();
        int z = params[3].toInt();
        //double offset = params[4].toDouble();
        Vector3 axis(x?1:0, y?1:0, z?1:0);

        for(int i = 1; i < count; i++)
        {
            auto cloneNode = n->clone();

            double theta = ((2.0 * M_PI) / count) * i;

            // Apply duplication operation
            {
                auto rotatePoint = [&](double theta, Vector3 v, Vector3 axis){
                    //if(theta == 0) return v;
                    //return (v * cos(theta) + axis.cross(v) * sin(theta) + axis * axis.dot(v) * (1 - cos(theta)));
                    Eigen::AngleAxisd q(theta, axis);
                    return q * v;
                };

                // Apply rotation to node geometry
                auto nodeGeometry = cloneNode->controlPoints();
                for(auto & p : nodeGeometry)
                {
                    p = rotatePoint(theta, p, axis);
                }
                cloneNode->setControlPoints(nodeGeometry);

                // Clone mesh and apply translation
                auto mesh = getMesh(n->id);
                auto cloneMesh = mesh->clone();
                for(auto v : cloneMesh->vertices())
                {
                    auto & p = cloneMesh->vertex_coordinates()[v];
                    p = rotatePoint(theta, p, axis);
                }
                cloneMesh->update_face_normals();
                cloneMesh->update_vertex_normals();
                cloneMesh->updateBoundingBox();
                cloneNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(cloneMesh));
            }

            result.push_back(cloneNode);
        }
    }

    char randomAlpha = toupper(97 + rand() % 26);

    for(int i = 0; i < result.size(); i++)
    {
        auto cloneNode = result[i];
        cloneNode->id = n->id + randomAlpha + QString::number(i+2);
        cloneNode->property["mesh_filename"].setValue(QString("meshes/%1.obj").arg(cloneNode->id));
    }

    return result;
}

void Model::duplicateActiveNodeViz(QString duplicationOp)
{
    if(activeNode == nullptr) return;

    tempNodes.clear();

    // Check for grouping option
    QStringList params = duplicationOp.split(",", QString::SkipEmptyParts);
    if(params.empty()) return;

    auto dups = makeDuplicates(activeNode, duplicationOp);

    for(auto n : dups)
    {
        // Distinguish new nodes
        auto color = n->vis_property["color"].value<QColor>();
        color = color.lighter(50);
        n->vis_property["color"].setValue(color);

        tempNodes.push_back(QSharedPointer<Structure::Node>(n));
    }

    // Hide previous group during visualization
    for(auto g : groupsOf(activeNode->id)){
        for(auto nid : g){
            if(nid == activeNode->id) continue;

            getNode(nid)->vis_property["isHidden"].setValue(params.back() == "group");
        }
    }
}

void Model::duplicateActiveNode(QString duplicationOp)
{
    if(activeNode == nullptr) return;

    // Remove visualizations
    tempNodes.clear();

    // Check for grouping option
    QStringList params = duplicationOp.split(",", QString::SkipEmptyParts);
    if(params.empty()) return;

    // Remove any previous groups
    if(params.back() == "group"){
        for(auto g : groupsOf(activeNode->id)){
            for(auto nid : g){
                if(nid == activeNode->id) continue;
                removeNode(nid);
            }
        }
    }

    // Create duplicate nodes
    auto dups = makeDuplicates(activeNode, duplicationOp);

    // Add nodes to graph and to a new group
    QVector<QString> nodesInGroup;
    nodesInGroup << activeNode->id;

    for(auto n : dups)
    {
        addNode(n);
        nodesInGroup << n->id;
    }

    if(params.back() == "group")
        addGroup(nodesInGroup);
}

void Model::modifyLastAdded(QVector<QVector3D> &guidePoints)
{
    if (guidePoints.size() < 2 || activeNode == nullptr) return;

    Structure::Curve* curve = dynamic_cast<Structure::Curve*>(activeNode);
    Structure::Sheet* sheet = dynamic_cast<Structure::Sheet*>(activeNode);

    auto fp = guidePoints.takeFirst();
    Vector3 axis(fp.x(),fp.y(),fp.z());

    int skipAxis = -1;
    if (axis.isApprox(-Vector3::UnitX())) skipAxis = 0;
    if (axis.isApprox(-Vector3::UnitY())) skipAxis = 1;
    if (axis.isApprox(Vector3::UnitZ())) skipAxis = 2;

    // Smooth curve
    guidePoints = GeometryHelper::smooth(GeometryHelper::uniformResampleCount(guidePoints, 20), 5);

    // Convert to Vector3
    Array1D_Vector3 gpoint;
    for(auto p : guidePoints) gpoint.push_back(Vector3(p[0],p[1],p[2]));

    if(curve)
    {
        auto oldPoints = curve->controlPoints();
        auto newPoints = oldPoints;

        for(size_t i = 0; i < oldPoints.size(); i++){
            for(int j = 0; j < 3; j++){
                if(j == skipAxis) newPoints[i][j] = oldPoints[i][j];
                else newPoints[i][j] = gpoint[i][j];
            }
        }

        curve->setControlPoints(newPoints);
    }

    if(sheet)
    {
        auto oldPoints = sheet->controlPoints();
        auto newPoints = oldPoints;

        Vector3 centroid = sheet->position(Eigen::Vector4d(0.5, 0.5, 0, 0));
        Vector3 delta = gpoint.front() - centroid;

        for(size_t i = 0; i < oldPoints.size(); i++){
            for (int j = 0; j < 3; j++){
                if (j == skipAxis) continue;
                newPoints[i][j] += delta[j];
            }
        }

        sheet->setControlPoints(newPoints);
        sheet->surface.quads.clear();
    }

    generateSurface();
}

void Model::generateSurface(double offset)
{
    ModelMesher mesher(this);

    //mesher.generateOffsetSurface(offset);
    mesher.generateRegularSurface(offset);
}

void Model::placeOnGround()
{
    this->normalize();
    this->moveBottomCenterToOrigin();
}

void Model::selectPart(QVector3D orig, QVector3D dir)
{
    QMap< double, QPair<int, Vector3> > isects;
    QMap< double, QString > isectNode;

    Vector3 origin (orig[0], orig[1], orig[2]);
    Vector3 direction (dir[0], dir[1], dir[2]);
    Vector3 ipoint;

    for(auto n : nodes)
    {
        auto nodeMesh = getMesh(n->id);
        if(!nodeMesh) continue;

        nodeMesh->updateBoundingBox();

        for(auto f : nodeMesh->faces())
        {
            std::vector<Vector3> tri;
            for(auto v : nodeMesh->vertices(f))
                tri.push_back( nodeMesh->vertex_coordinates()[v] );

            if( GeometryHelper::intersectRayTri(tri, origin, direction, ipoint) )
            {
                double dist = (origin - ipoint).norm();
                isects[dist] = qMakePair(f.idx(), ipoint);
                isectNode[dist] = n->id;
            }
        }
    }

    // Only consider closest hit
    if(isects.size())
    {
        double minDist = isects.keys().front();
        activeNode = getNode(isectNode[minDist]);
    }
    else
    {
        activeNode = nullptr;
    }
}

void Model::deselectAll()
{
    activeNode = nullptr;
    tempNodes.clear();
}

void Model::draw(Viewer *glwidget)
{
    // Collect meshes
    QVector<SurfaceMeshModel *> meshes;
    for(auto n : nodes){
        auto mesh = getMesh(n->id);
        if(mesh != nullptr) { meshes << mesh; } else
        {
            if(n->type() == Structure::CURVE)
            {
                // Draw as a basic 3D curve
                auto nodeColor = n->vis_property["color"].value<QColor>();
                QVector<QVector3D> lines;
                auto points = n->controlPoints();
                for(int i = 1; i < points.size(); i++){
                    lines << QVector3D(points[i-1][0],points[i-1][1],points[i-1][2]);
                    lines << QVector3D(points[i][0],points[i][1],points[i][2]);
                }
                glwidget->glLineWidth(6);
                glwidget->drawLines(lines, nodeColor, glwidget->pvm, "lines");
            }

            if(n->type() == Structure::SHEET)
            {
                auto & sheet = ((Structure::Sheet*) n)->surface;

                // Build surface geometry if needed
                if(sheet.quads.empty())
                {
                    double resolution = (sheet.mCtrlPoint.front().front()
                                         - sheet.mCtrlPoint.back().back()).norm() * 0.1;
                    sheet.generateSurfaceQuads( resolution );
                }

                // Draw surface as triangles
                QVector<QVector3D> points, normals;
                for(auto quad : sheet.quads){
                    QVector<QVector3D> quad_points, quad_normals;

                    for(int i = 0; i < 4; i++){
                        quad_points << QVector3D(quad.p[i][0],quad.p[i][1],quad.p[i][2]);
                        quad_normals << QVector3D(quad.n[i][0],quad.n[i][1],quad.n[i][2]);
                    }

                    // Add two triangles of the quad
                    points<<quad_points[0]; normals<<quad_normals[0];
                    points<<quad_points[1]; normals<<quad_normals[1];
                    points<<quad_points[2]; normals<<quad_normals[2];

                    points<<quad_points[0]; normals<<quad_normals[0];
                    points<<quad_points[2]; normals<<quad_normals[2];
                    points<<quad_points[3]; normals<<quad_normals[3];
                }

                auto nodeColor = n->vis_property["color"].value<QColor>();
                glwidget->drawTriangles(nodeColor, points, normals, glwidget->pvm);
                glwidget->drawPoints(points,nodeColor,glwidget->pvm);
            }
        }
    }

    if(meshes.empty()) return;

    // Draw meshes:
    glwidget->glEnable(GL_DEPTH_TEST);
    //glwidget->glEnable(GL_CULL_FACE);
    glwidget->glCullFace(GL_BACK);

    // Activate shader
    auto & program = *glwidget->shaders["mesh"];
    program.bind();

    // Attributes
    int vertexLocation = program.attributeLocation("vertex");
    int normalLocation = program.attributeLocation("normal");
    int colorLocation = program.attributeLocation("color");

    program.enableAttributeArray(vertexLocation);
    program.enableAttributeArray(normalLocation);
    program.enableAttributeArray(colorLocation);

    // Uniforms
    int matrixLocation = program.uniformLocation("matrix");
    int lightPosLocation = program.uniformLocation("lightPos");
    int viewPosLocation = program.uniformLocation("viewPos");
    int lightColorLocation = program.uniformLocation("lightColor");

    program.setUniformValue(matrixLocation, glwidget->pvm);
    program.setUniformValue(lightPosLocation, glwidget->eyePos);
    program.setUniformValue(viewPosLocation, glwidget->eyePos);
    program.setUniformValue(lightColorLocation, QVector3D(1,1,1));

    // Add visualized nodes for duplication and such
    auto allNodes = nodes;
    for(auto n : tempNodes) allNodes.push_back(n.data());

    // Draw parts as meshes
    for(auto n : allNodes)
    {
        auto mesh = n->property["mesh"].value< QSharedPointer<SurfaceMeshModel> >().data();
        if(mesh == nullptr || mesh->n_faces() < 1) continue;
        if(n->vis_property["isHidden"].toBool()) continue;

        auto nodeColor = n->vis_property["color"].value<QColor>();
        auto ncolor = Eigen::Vector3f(nodeColor.redF(), nodeColor.greenF(), nodeColor.blueF());

        bool isSmoothShading = n->vis_property["isSmoothShading"].toBool();

        // Pack geometry, normals, and colors
        QVector<GLfloat> vertex, normal, color;

        auto mesh_points = mesh->vertex_coordinates();
        auto mesh_normals = mesh->vertex_normals();
        auto mesh_fnormals = mesh->face_normals();

        int v_count = 0;

        // Pack mesh faces
        for(auto f : mesh->faces()){
            for(auto vf : mesh->vertices(f)){
                for(int i = 0; i < 3; i++){
                    // Coordinate
                    vertex << mesh_points[vf][i];
                    // Color
                    color << ncolor[i];
                    // Normal
                    if (!isSmoothShading) normal << mesh_fnormals[f][i];
                    else normal << mesh_normals[vf][i];
                    v_count++;
                }
            }
        }

        // Shader data
        program.setAttributeArray(vertexLocation, &vertex[0], 3);
        program.setAttributeArray(normalLocation, &normal[0], 3);
        program.setAttributeArray(colorLocation, &color[0], 3);

        // Draw
        glwidget->glDrawArrays(GL_TRIANGLES, 0, mesh->n_faces() * 3);
    }

    program.disableAttributeArray(vertexLocation);
    program.disableAttributeArray(normalLocation);
    program.disableAttributeArray(colorLocation);

    program.release();

    // Draw bounding box around active part
    if(activeNode != nullptr && getMesh(activeNode->id) != nullptr)
    {
        auto mesh = getMesh(activeNode->id);
        auto box = mesh->bbox();

        QVector<Eigen::Vector3d> corners;
        corners<<box.corner(Eigen::AlignedBox3d::BottomLeftFloor);
        corners<<box.corner(Eigen::AlignedBox3d::BottomRightFloor);
        corners<<box.corner(Eigen::AlignedBox3d::TopLeftFloor);
        corners<<box.corner(Eigen::AlignedBox3d::TopRightFloor);
        corners<<box.corner(Eigen::AlignedBox3d::BottomLeftCeil);
        corners<<box.corner(Eigen::AlignedBox3d::BottomRightCeil);
        corners<<box.corner(Eigen::AlignedBox3d::TopLeftCeil);
        corners<<box.corner(Eigen::AlignedBox3d::TopRightCeil);

        QVector<QVector3D> lines;
        auto addLine = [&](Eigen::Vector3d a, Eigen::Vector3d b){
            Vector3 dir = (a - b).normalized() * 0.025;
            lines << toQVector3D(a);
            lines << toQVector3D(a - dir);
            lines << toQVector3D(b);
            lines << toQVector3D(b + dir);
        };

        addLine(corners[0], corners[1]);
        addLine(corners[0], corners[2]);
        addLine(corners[0], corners[4]);
        addLine(corners[1], corners[3]);
        addLine(corners[1], corners[5]);
        addLine(corners[2], corners[3]);
        addLine(corners[2], corners[6]);
        addLine(corners[3], corners[7]);
        addLine(corners[4], corners[5]);
        addLine(corners[4], corners[6]);
        addLine(corners[5], corners[7]);
        addLine(corners[6], corners[7]);

        QColor color(255,255,255,150);

        glwidget->glLineWidth(2);
        glwidget->drawLines(lines, color, glwidget->pvm, "lines");
    }

    glwidget->glDisable(GL_DEPTH_TEST);
    //glwidget->glDisable(GL_CULL_FACE);

    if(ShapeGraph::property["showEdges"].toBool())
    {
        QVector<QVector3D> lines;

        for(auto e : edges)
        {
            lines << toQVector3D(e->position(e->n1->id));
            lines << toQVector3D(e->position(e->n2->id));
        }

        glwidget->glLineWidth(4);
        QColor color(255,0,255,100);
        glwidget->drawLines(lines, color, glwidget->pvm, "lines");
    }
}

void Model::storeActiveNodeGeometry()
{
    if (activeNode == nullptr) return;

    QSet<Structure::Node*> nodes;
    nodes << activeNode;

    // Apply to group if in any
    for(auto g : groupsOf(activeNode->id)){
        for(auto nid : g){
            nodes << getNode(nid);
        }
    }

    for(auto n : nodes)
    {
        // Store initial node and mesh geometries
        n->property["restNodeGeometry"].setValue(n->controlPoints());
        n->property["restNodeCentroid"].setValue(n->center());
        auto mesh = getMesh(n->id);
        Array1D_Vector3 meshPoints;
        for (auto v : mesh->vertices()) meshPoints.push_back(mesh->vertex_coordinates()[v]);
        n->property["restMeshGeometry"].setValue(meshPoints);
    }
}

void Model::transformActiveNodeGeometry(QMatrix4x4 transform)
{
    if (activeNode == nullptr) return;
    if (!activeNode->property.contains("restNodeGeometry")) return;

    QSet<Structure::Node*> nodes;
    nodes << activeNode;

    // Apply to group if in any
    for(auto g : groupsOf(activeNode->id)){
        for(auto nid : g){
            nodes << getNode(nid);
        }
    }

    for(auto n : nodes)
    {
        auto n_centroid = n->property["restNodeCentroid"].value<Vector3>();

        // Apply transformation on rest geometry
        auto nodeGeometry = n->property["restNodeGeometry"].value<Array1D_Vector3>();
        for(auto & p : nodeGeometry)
        {
            Vector3 q = starlab::QVector3(transform * starlab::QVector3((p - n_centroid)));
            p = q + n_centroid;
        }
        n->setControlPoints(nodeGeometry);

        auto mesh = getMesh(n->id);
        auto meshPoints = n->property["restMeshGeometry"].value<Array1D_Vector3>();
        for (auto v : mesh->vertices()){
            Vector3 p = meshPoints[v.idx()] - n_centroid;
            p = starlab::QVector3(transform * starlab::QVector3(p));
            mesh->vertex_coordinates()[v] = p + n_centroid;
        }

        mesh->update_face_normals();
        mesh->update_vertex_normals();
        mesh->updateBoundingBox();
    }
}

Structure::ShapeGraph* Model::cloneAsShapeGraph()
{
    auto clone = new Structure::ShapeGraph(name());
    for(auto n : nodes)
    {
        auto cloneNode = clone->addNode(n->clone());

        // clone meshes as well
        auto mesh = getMesh(n->id);
        auto cloneMesh = mesh->clone();
        cloneMesh->update_face_normals();
        cloneMesh->update_vertex_normals();
        cloneMesh->updateBoundingBox();
        cloneNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(cloneMesh));
    }
    for(auto e : edges){
        Structure::Node * n1 = clone->getNode(e->n1->id);
        Structure::Node * n2 = clone->getNode(e->n2->id);
        Structure::Link * newEdge = clone->addEdge(n1, n2, e->coord[0], e->coord[1], e->id);
        newEdge->property = e->property;
    }
    clone->groups = groups;
    clone->property = ((Structure::ShapeGraph*)this)->property;
    clone->misc = misc;
    clone->debugPoints = debugPoints;
    clone->debugPoints2 = debugPoints2;
    clone->debugPoints3 = debugPoints3;
    clone->vs = vs; clone->vs2 = vs2; clone->vs3 = vs3;
    clone->ps = ps; clone->ps2 = ps2; clone->ps3 = ps3;
    clone->spheres = spheres; clone->spheres2 = spheres2;
    clone->ueid = ueid;
    clone->landmarks = landmarks;
    clone->relations = relations;
    clone->animation = animation;
    clone->animation_index = animation_index;
    clone->animation_debug = animation_debug;
    return clone;
}

QString Model::name()
{
	return QFileInfo(Graph::property["name"].toString()).dir().dirName();
}
