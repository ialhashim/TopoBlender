#include "Model.h"

Model::Model(QObject *parent) : QObject(parent), Structure::ShapeGraph("")
{

}

void Model::draw(Viewer *glwidget)
{
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

