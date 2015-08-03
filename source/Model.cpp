#include "Model.h"
using namespace opengp;

#include "GeometryHelper.h"

Model::Model(QObject *parent) : QObject(parent), Structure::ShapeGraph("")
{

}

void Model::createCurveFromPoints(QVector<QVector3D> & points)
{
    // Nicer curve
    points = GeometryHelper::smooth(GeometryHelper::uniformResampleCount(points, 20), 4);

    // Create curve
    Array1D_Vector3 controlPoints;
    for(auto p : points) controlPoints.push_back(Vector3(p[0],p[1],p[2]));

    auto newCurveGeometry = NURBS::NURBSCurved::createCurveFromPoints(controlPoints);
    QString newCurveID = QString("curveNode%1").arg(nodes.back()->property["index"].toInt()+1);
    auto newCurve = new Structure::Curve(newCurveGeometry, newCurveID);

    // Add curve
    this->addNode(newCurve);
}

void Model::createSheetFromPoints(QVector<QVector3D> &points)
{
    // Convert corner points to Vector3
    Array1D_Vector3 cornerPoints;
    for(auto p : points) cornerPoints.push_back(Vector3(p[0],p[1],p[2]));

    // Create sheet
    auto newSheetGeometry = NURBS::NURBSRectangled::createSheet(cornerPoints.front(), cornerPoints.back(),8,8);
    QString newSheetID = QString("sheetNode%1").arg(nodes.back()->property["index"].toInt()+1);
    auto newSheet = new Structure::Sheet(newSheetGeometry, newSheetID);

    // Add sheet
    this->addNode(newSheet);
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

                if(sheet.quads.empty())
                {
                    double resolution = (sheet.mCtrlPoint.front().front()
                                         - sheet.mCtrlPoint.back().back()).norm() * 0.05;
                    sheet.generateSurfaceQuads( resolution );
                }

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
        if(mesh == nullptr) continue;

        auto nodeColor = n->vis_property["color"].value<QColor>();
        auto ncolor = Eigen::Vector3f(nodeColor.redF(), nodeColor.greenF(), nodeColor.blueF());

        // Pack geometry, normals, and colors
        QVector<GLfloat> vertex, normal, color;

        auto mesh_points = mesh->vertex_coordinates();
        auto mesh_normals = mesh->vertex_normals();
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

