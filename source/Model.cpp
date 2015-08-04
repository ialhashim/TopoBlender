#include "Model.h"
#include "Viewer.h"
#include "GeometryHelper.h"

#define SDFGEN_HEADER_ONLY
#include "makelevelset3.h"
#include "marchingcubes.h"

using namespace opengp;

Model::Model(QObject *parent) : QObject(parent), Structure::ShapeGraph(""), lastAddedNode(nullptr)
{

}

void Model::createCurveFromPoints(QVector<QVector3D> & points)
{
    if(points.empty()) return;

    // Nicer curve
    points = GeometryHelper::smooth(GeometryHelper::uniformResampleCount(points, 20), 5);

    // Create curve
    Array1D_Vector3 controlPoints;
    for(auto p : points) controlPoints.push_back(Vector3(p[0],p[1],p[2]));

    auto newCurveGeometry = NURBS::NURBSCurved::createCurveFromPoints(controlPoints);
    QString newCurveID = QString("curveNode%1").arg((nodes.empty()? -1 : nodes.back()->property["index"].toInt())+1);
    auto newCurve = new Structure::Curve(newCurveGeometry, newCurveID);

    // Add curve
    lastAddedNode = this->addNode(newCurve);

    generateSurface(0.03);
}

void Model::createSheetFromPoints(QVector<QVector3D> &points)
{
    if(points.empty()) return;

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
    lastAddedNode = this->addNode(newSheet);

    generateSurface(0.03);
}

void Model::modifyLastAdded(QVector<QVector3D> &guidePoints)
{
    if(guidePoints.empty() || lastAddedNode == nullptr) return;

    Structure::Curve* curve = dynamic_cast<Structure::Curve*>(lastAddedNode);
    Structure::Sheet* sheet = dynamic_cast<Structure::Sheet*>(lastAddedNode);

    auto fp = guidePoints.takeFirst();
    Vector3 axis(fp.x(),fp.y(),fp.z());

    // Smooth curve
    guidePoints = GeometryHelper::smooth(GeometryHelper::uniformResampleCount(guidePoints, 20), 5);

    // Convert
    Array1D_Vector3 gpoint;
    for(auto p : guidePoints) gpoint.push_back(Vector3(p[0],p[1],p[2]));

    if(curve)
    {
        int skipAxis = -1;
        if(axis.isApprox(-Vector3::UnitX())) skipAxis = 0;
        if(axis.isApprox(-Vector3::UnitY())) skipAxis = 1;
        if(axis.isApprox( Vector3::UnitZ())) skipAxis = 2;

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

        Vector3 delta = gpoint.front();

        // User surface normal to determain axis
        Vector3 pos(0,0,0), t0(0,0,0), t1(0,0,0), normal(0,0,0);
        sheet->surface.GetFrame(0.5,0.5, pos, t0,t1, normal);
        normal.normalize();
        axis = normal;

        int skipAxis = -1;
        if(axis.isApprox(-Vector3::UnitX())) skipAxis = 0;
        if(axis.isApprox(-Vector3::UnitY())) skipAxis = 1;
        if(axis.isApprox( Vector3::UnitZ())) skipAxis = 2;

        for(size_t i = 0; i < oldPoints.size(); i++){
            for(int j = 0; j < 3; j++){
                if(j == skipAxis){
                    newPoints[i][j] = delta[j];
                }
            }
        }

        sheet->setControlPoints(newPoints);
        sheet->surface.quads.clear();
    }

    generateSurface(0.03);
}

void Model::generateSurface(double offset)
{
    if(lastAddedNode == nullptr) return;

    //for(auto n : nodes)
    auto n = lastAddedNode;
    {
        double dx = 0.015;

        std::vector<SDFGen::Vec3f> vertList;
        std::vector<SDFGen::Vec3ui> faceList;

        //start with a massive inside out bound box.
        SDFGen::Vec3f min_box(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()),
            max_box(-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),-std::numeric_limits<float>::max());

        Structure::Curve* curve = dynamic_cast<Structure::Curve*>(n);
        Structure::Sheet* sheet = dynamic_cast<Structure::Sheet*>(n);

        //generate "tri-mesh" from skeleton geometry
        if(curve)
        {
            QVector<QVector3D> cpts;
            for(auto p : curve->controlPoints()) cpts << QVector3D(p[0],p[1],p[2]);
            cpts = GeometryHelper::uniformResampleCount(cpts, cpts.size() * 5);

            int vi = 0;

            for(size_t i = 1; i < cpts.size(); i++){
                SDFGen::Vec3f p0 (cpts[i-1][0],cpts[i-1][1],cpts[i-1][2]);
                SDFGen::Vec3f p1 (cpts[i][0],cpts[i][1],cpts[i][2]);
                SDFGen::Vec3f p2 = (p0 + p1) * 0.5;

                vertList.push_back(p0);
                vertList.push_back(p1);
                vertList.push_back(p2);

                faceList.push_back(SDFGen::Vec3ui(vi+0,vi+1,vi+2));
                vi += 3;

                update_minmax(p0, min_box, max_box);
                update_minmax(p1, min_box, max_box);
                update_minmax(p2, min_box, max_box);
            }
        }

        if(sheet)
        {
            // Build surface geometry if needed
            auto & surface = sheet->surface;
            if(surface.quads.empty()){
                double resolution = (surface.mCtrlPoint.front().front()
                                     - surface.mCtrlPoint.back().back()).norm() * 0.1;
                surface.generateSurfaceQuads( resolution );
            }

            int vi = 0;

            for(auto quad : surface.quads)
            {
                QVector<SDFGen::Vec3f> p;
                for(int i = 0; i < 4; i++){
                    SDFGen::Vec3f point(quad.p[i][0],quad.p[i][1],quad.p[i][2]);
                    p << point;
                    update_minmax(point, min_box, max_box);
                }

                vertList.push_back(p[0]);
                vertList.push_back(p[1]);
                vertList.push_back(p[2]);
                faceList.push_back(SDFGen::Vec3ui(vi+0,vi+1,vi+2));
                vi += 3;

                vertList.push_back(p[0]);
                vertList.push_back(p[2]);
                vertList.push_back(p[3]);
                faceList.push_back(SDFGen::Vec3ui(vi+0,vi+1,vi+2));
                vi += 3;
            }
        }

        int padding = 10;
        SDFGen::Vec3f unit(1,1,1);
        min_box -= unit*padding*dx;
        max_box += unit*padding*dx;
        SDFGen::Vec3ui sizes = SDFGen::Vec3ui((max_box - min_box)/dx);

        Array3f phi_grid;
        SDFGen::make_level_set3(faceList, vertList, min_box, dx, sizes[0], sizes[1], sizes[2], phi_grid);

        ScalarVolume volume = initScalarVolume(sizes[0],sizes[1],sizes[2], 1e6);

        #pragma omp for
        for(int i = 0; i < sizes[0]; i++){
            for(int j = 0; j < sizes[1]; j++){
                for(int k = 0; k < sizes[2]; k++){
                    volume[k][j][i] = phi_grid(i,j,k);
                }
            }
        }

        auto mesh = march(volume, offset);

        QSharedPointer<SurfaceMeshModel> newMesh = QSharedPointer<SurfaceMeshModel>(new SurfaceMeshModel());

        Vector3 halfUnite = 0.5 * Vector3(dx,dx,dx);

        int vi = 0;
        for(auto tri : mesh){
            std::vector<SurfaceMeshModel::Vertex> verts;
            for(auto p : tri){
                Vector3 voxel(p.x, p.y, p.z);

                Vector3 pos = (voxel * dx) + Vector3(min_box[0],min_box[1],min_box[2]) + halfUnite;

                newMesh->add_vertex(pos);

                verts.push_back(SurfaceMeshModel::Vertex(vi++));
            }
            newMesh->add_face(verts);
        }

        newMesh->updateBoundingBox();
        newMesh->update_face_normals();
        newMesh->update_vertex_normals();

        n->property["mesh"].setValue(newMesh);
    }
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
    glwidget->glEnable(GL_CULL_FACE);
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

    // Draw parts as meshes
    for(auto n : nodes)
    {
        auto mesh = getMesh(n->id);
        if(mesh == nullptr || mesh->n_faces() < 1) continue;

        auto nodeColor = n->vis_property["color"].value<QColor>();
        auto ncolor = Eigen::Vector3f(nodeColor.redF(), nodeColor.greenF(), nodeColor.blueF());

        // Pack geometry, normals, and colors
        QVector<GLfloat> vertex, normal, color;

        auto mesh_points = mesh->vertex_coordinates();
        //auto mesh_normals = mesh->vertex_normals();
        auto mesh_fnormals = mesh->face_normals();

        int v_count = 0;

        for(auto f : mesh->faces()){
            for(auto vf : mesh->vertices(f)){
                for(int i = 0; i < 3; i++){
                    vertex << mesh_points[vf][i];
                    //normal << mesh_normals[vf][i];
                    normal << mesh_fnormals[f][i];
                    color << ncolor[i];
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

    glwidget->glDisable(GL_DEPTH_TEST);
    glwidget->glDisable(GL_CULL_FACE);
}

