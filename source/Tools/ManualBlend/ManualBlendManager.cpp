#include "ManualBlendManager.h"
#include "Document.h"
#include "Model.h"

#include "GraphCorresponder.h"
#include "TopoBlender.h"
#include "Scheduler.h"
#include "SynthesisManager.h"

#include "SchedulerWidget.h"
#include <QMainWindow>

#include "poissonrecon.h"

ManualBlendManager::ManualBlendManager(Document *document) : document(document), isBusy(false)
{

}

void ManualBlendManager::doBlend(QString source_part_name, QString target_name, QString target_part_name, int value)
{
	if (sourcePartName != source_part_name || targetName != target_name || targetPartName != target_part_name)
	{
		this->sourcePartName = source_part_name;
		this->targetName = target_name;
		this->targetPartName = target_part_name;

		auto source = document->getModel(document->firstModelName());
		auto target = document->cacheModel(targetName);

		int numSamples = 2500;

		gcorr = QSharedPointer<GraphCorresponder>(new GraphCorresponder(source, target));

		for (auto n : source->nodes)
		{
			QString sid = n->id;

			if (sid == sourcePartName)
			{
				gcorr->addLandmarks(QVector<QString>() << sourcePartName, QVector<QString>() << targetPartName);
			}
			else
			{
				if (document->datasetCorr[sid][targetName].empty())
				{
					gcorr->setNonCorresSource(sid);
				}
				else
				{
					auto tid = document->datasetCorr[sid][targetName].front();
					gcorr->addLandmarks(QVector<QString>() << sid, QVector<QString>() << tid);
				}
			}
		}

		gcorr->computeCorrespondences();

		scheduler = QSharedPointer<Scheduler>(new Scheduler);
		blender = QSharedPointer<TopoBlender>(new TopoBlender(gcorr.data(), scheduler.data()));
		synthManager = QSharedPointer<SynthesisManager>(new SynthesisManager(gcorr.data(), scheduler.data(), blender.data(), numSamples));

		/// Reschedule:
		{
			// Move task of part to start
			for (auto n : scheduler->activeGraph->nodes){
				QString sid = n->id;
				if (sid == sourcePartName || source->shareGroup(sid, sourcePartName))
					scheduler->moveTaskToStart(sid);
				else
					n->property["skipSynth"].setValue(true);
			}

			// Move remaining to end
			int endTime = scheduler->endOf(QList<Task*>() << scheduler->getTaskFromNodeID(sourcePartName));
			scheduler->moveAllButTaskToTime(sourcePartName, endTime + 100);
		}

		/// Visualize schedule:
		//blender->parentWidget = new QMainWindow();
		//blender->parentWidget->show();
		//blender->setupUI();

		/// Sample geometry
		synthManager->genSynData();

		/// Compute blending
		scheduler->property["forceStopTime"].setValue(0.5);
		scheduler->timeStep = 1.0 / 20.0;
		scheduler->executeAll();
	}

	if (!synthManager.isNull() && !isBusy)
	{
		isBusy = true;

		double t = double(value) / 100.0;
		auto t_graph = scheduler->allGraphs[t * (scheduler->allGraphs.size() - 1)];
		auto cloud = synthManager->reconstructGeometryNode(t_graph->getNode(sourcePartName), t);
		
		emit(cloudReady(cloud));

		isBusy = false;
	}
}

void ManualBlendManager::finalizeBlend(QString source_part_name, QString target_name, QString target_part_name, int value)
{
	double t = double(value) / 100.0;
	auto t_graph = scheduler->allGraphs[t * (scheduler->allGraphs.size() - 1)];
	auto t_node = t_graph->getNode(source_part_name);

	// Get point cloud
	auto cloud = synthManager->reconstructGeometryNode(t_node, t);

	// Mesh the cloud
	SimpleMesh mesh;
	std::vector< std::vector<float> > finalP, finalN;
	auto cp = cloud.first, cn = cloud.second;
	for (auto p : cp){
		std::vector<float> point;
		point.push_back(p.x()); point.push_back(p.y()); point.push_back(p.z());
		finalP.push_back(point);
	}
	for (auto n : cn){
		std::vector<float> normal;
		normal.push_back(n.x()); normal.push_back(n.y()); normal.push_back(n.z());
		finalN.push_back(normal);
	}
	PoissonRecon::makeFromCloud(finalP, finalN, mesh, 6);

	// Copy results
	auto nodeMesh = QSharedPointer<SurfaceMeshModel>(new SurfaceMeshModel(source_part_name));

	// Vertices
	for (int i = 0; i < (int)mesh.vertices.size(); i++)
	{
		const std::vector<float> & p = mesh.vertices[i];
		nodeMesh->add_vertex(Vector3(p[0], p[1], p[2]));
	}
	// Faces
	for (int i = 0; i < (int)mesh.faces.size(); i++)
	{
		std::vector<SurfaceMeshModel::Vertex> face;
		for (int vi = 0; vi < 3; vi++) face.push_back(SurfaceMeshModel::Vertex(mesh.faces[i][vi]));
		nodeMesh->add_face(face);
	}

	nodeMesh->update_face_normals();
	nodeMesh->update_vertex_normals();
	nodeMesh->updateBoundingBox();

	// Replace the node
	auto model = document->getModel(document->firstModelName());
	auto n = model->getNode(source_part_name);

	model->removeNode(n->id);
	auto newNode = model->addNode(t_node->clone());
	newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
}
