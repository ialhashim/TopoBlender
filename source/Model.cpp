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

    // Draw parts as meshes
    for(auto n : nodes)
    {
        auto mesh = getMesh(n->id);
        if(mesh == nullptr || mesh->n_faces() < 1) continue;

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

    glwidget->glDisable(GL_DEPTH_TEST);
    //glwidget->glDisable(GL_CULL_FACE);
}

void Model::storeActiveNodeGeometry()
{
	if (activeNode == nullptr) return;

	// Store initial node and mesh geometries
	auto n = activeNode;
	n->property["restNodeGeometry"].setValue(n->controlPoints());
	n->property["restNodeCentroid"].setValue(n->center());
	auto mesh = getMesh(n->id);
	Array1D_Vector3 meshPoints;
	for (auto v : mesh->vertices()) meshPoints.push_back(mesh->vertex_coordinates()[v]);
	n->property["restMeshGeometry"].setValue(meshPoints);
}

void Model::transformActiveNodeGeometry(QMatrix4x4 transform)
{
	if (activeNode == nullptr) return;
	if (!activeNode->property.contains("restNodeGeometry")) return;

	// Apply transformation on rest geometry
	auto n = activeNode;
	n->setControlPoints(n->property["restNodeGeometry"].value<Array1D_Vector3>());
	auto mesh = getMesh(n->id);
	auto meshPoints = n->property["restMeshGeometry"].value<Array1D_Vector3>();
	auto n_centroid = n->property["restNodeCentroid"].value<Vector3>();
	for (auto v : mesh->vertices()){
		Vector3 p = meshPoints[v.idx()] - n_centroid;
		p = starlab::QVector3(transform * starlab::QVector3(p));
		mesh->vertex_coordinates()[v] = p + n_centroid;
	}

	mesh->update_face_normals();
	mesh->update_vertex_normals();
	mesh->updateBoundingBox();
}
