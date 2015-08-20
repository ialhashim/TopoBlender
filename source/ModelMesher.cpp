#include "ModelMesher.h"
#include "Model.h"
#include "GeometryHelper.h"

#include "weld.h"

#define SDFGEN_HEADER_ONLY
#include "makelevelset3.h"
#include "marchingcubes.h"

#include "RMF.h"

ModelMesher::ModelMesher(Model *model) : m(model)
{

}

void ModelMesher::generateOffsetSurface(double offset)
{
    if(m->activeNode == nullptr) return;
    auto n = m->activeNode;
    n->vis_property["isSmoothShading"].setValue(true);

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
		// Lower resolution for sheets
		dx *= 2.0;

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

    if (faceList.empty() || vertList.empty()) return;

    Array3f phi_grid;
    SDFGen::make_level_set3(faceList, vertList, min_box, dx, sizes[0], sizes[1], sizes[2], phi_grid, false, offset * 2.0);

    ScalarVolume volume = initScalarVolume(sizes[0], sizes[1], sizes[2], (sizes[0] + sizes[1] + sizes[2])*dx);

    for(int i = 0; i < sizes[0]; i++){
        for(int j = 0; j < sizes[1]; j++){
            for(int k = 0; k < sizes[2]; k++){
                volume[k][j][i] = phi_grid(i,j,k);
            }
        }
    }

    // Mesh surface from volume using marching cubes
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

    GeometryHelper::meregeVertices<Vector3>(newMesh.data());

    newMesh->updateBoundingBox();
    newMesh->update_face_normals();
    newMesh->update_vertex_normals();

	n->property["mesh"].setValue(newMesh);
	n->property["mesh_filename"].setValue(QString("meshes/%1.obj").arg(n->id));
}

void ModelMesher::generateRegularSurface(double offset)
{
    if(m->activeNode == nullptr) return;
    auto n = m->activeNode;
    n->vis_property["isSmoothShading"].setValue(true);

    Structure::Curve* curve = dynamic_cast<Structure::Curve*>(n);
    Structure::Sheet* sheet = dynamic_cast<Structure::Sheet*>(n);

	QSharedPointer<SurfaceMeshModel> newMesh = QSharedPointer<SurfaceMeshModel>(new SurfaceMeshModel());

    if(curve)
    {
        int numSegments = 20;
		int radialSegments = 20;

		std::vector< std::vector<Vector3> > grid;

		RMF rmf(curve->discretizedAsCurve(curve->length() / numSegments));

		// Compute frames & construct grid
		for (int i = 0; i < numSegments; i++)
        {
			// Frames
			auto point = rmf.point[i];
			auto normal = rmf.U[i].r;
			auto binormal = rmf.U[i].s;
			auto tangent = rmf.U[i].t;

			// Construct grid
			double r = offset;

			// Start cap
			if (i == 0)
			{
				double d = M_PI * 2 / radialSegments;
				for (double phi = M_PI * 0.5; phi >= d; phi -= d){
					grid.push_back(std::vector<Vector3>());
					for (double theta = 0; theta < M_PI * 2; theta += d){
						auto q1 = Eigen::AngleAxisd(theta + M_PI, -tangent);
						auto q2 = Eigen::AngleAxisd(phi, binormal);
						grid.back().push_back( (q1 * (q2 * normal)) * r + point );
					}
				}
			}
			
			// Middle segments
			grid.push_back(std::vector<Vector3>());
			for (int j = 0; j < radialSegments; j++){
				double v = double(j) / radialSegments * 2.0 * M_PI;
				double cx = -r * std::cos(v);
				double cy = r * std::sin(v);
				auto pos2 = point;
				pos2.x() += cx * normal.x() + cy * binormal.x();
				pos2.y() += cx * normal.y() + cy * binormal.y();
				pos2.z() += cx * normal.z() + cy * binormal.z();
				grid.back().push_back(Vector3(pos2.x(), pos2.y(), pos2.z()));
			}

			// End cap
			if (i == numSegments - 1)
			{
				double d = M_PI * 2 / radialSegments;
				for (double phi = d; phi <= M_PI * 0.5; phi += d){
					grid.push_back(std::vector<Vector3>());
					for (double theta = 0; theta < M_PI * 2; theta += d){
						auto q1 = Eigen::AngleAxisd(theta + M_PI, -tangent);
						auto q2 = Eigen::AngleAxisd(phi, -binormal);
						grid.back().push_back((q1 * (q2 * normal)) * r + point);
					}
				}
			}
        }

		// Construct the mesh
		for (size_t i = 0; i < grid.size() - 1; i++)
		{
			for (size_t j = 0; j < grid.front().size(); j++)
			{
				auto  ip = i + 1;
				auto  jp = (j + 1) % grid.front().size();

				auto a = grid[i][j];
				auto b = grid[ip][j];
				auto c = grid[ip][jp];
				auto d = grid[i][jp];

				auto addTriangle = [&](Vector3 a, Vector3 b, Vector3 c){
					typedef SurfaceMeshModel::Vertex Vert;
					int v = newMesh->n_vertices();
					newMesh->add_vertex(a);
					newMesh->add_vertex(b);
					newMesh->add_vertex(c);
					newMesh->add_triangle(Vert(v + 0), Vert(v + 1), Vert(v + 2));
				};

				if (i != 0) addTriangle(a, b, d);
				if (i != grid.size() - 2) addTriangle(b, c, d);
			}
		}
    }

    if(sheet)
    {
		generateOffsetSurface(offset);
		return;
    }

	GeometryHelper::meregeVertices<Vector3>(newMesh.data());

	newMesh->updateBoundingBox();
	newMesh->update_face_normals();
	newMesh->update_vertex_normals();

	n->property["mesh"].setValue(newMesh);
	n->property["mesh_filename"].setValue(QString("meshes/%1.obj").arg(n->id));
}

