#include "ManualBlendManager.h"
#include "Document.h"
#include "Model.h"

#include "GraphCorresponder.h"
#include "TopoBlender.h"
#include "Scheduler.h"
#include "SynthesisManager.h"

#include "SchedulerWidget.h"
#include <QMainWindow>
#include <QMessageBox>

#include "poissonrecon.h"

ManualBlendManager::ManualBlendManager(Document *document) : document(document), isBusy(false)
{

}

void ManualBlendManager::doBlend(QString source_part_name, QString target_name, QString target_part_name, int value)
{
    int numSamples = 2500;

    // Reconstruction Options
    switch(property("reconResolution").toInt()){
    case 0: numSamples = 2500; break;
    case 1: numSamples = 8000; break;
    case 2: numSamples = 20000; break;
    }

// Speed up debugging
#ifdef DEBUG
	numSamples /= 50;
#endif // DEBUG

	if (sourcePartName != source_part_name || targetName != target_name || targetPartName != target_part_name)
	{
		this->sourcePartName = source_part_name;
		this->targetName = target_name;
		this->targetPartName = target_part_name;

        QString sourceName = document->firstModelName();

        auto source = document->getModel(sourceName)->cloneAsShapeGraph();
		auto target = document->cacheModel(targetName)->cloneAsShapeGraph();

		gcorr = QSharedPointer<GraphCorresponder>(new GraphCorresponder(source, target));

		QVector<QString> sids, tids;
		auto groups = source->groupsOf(sourcePartName);
		for (auto group : groups){
			for (auto id : group){
				sids << id;

				if (document->datasetCorr[sourceName][id][targetName].empty()){
					gcorr->setNonCorresSource(id);
				} else {
					auto tid = document->datasetCorr[sourceName][id][targetName].front();
					if (!tids.contains(tid))
						tids << tid;
				}
			}
		}

		// One-to-many?
		if (sids.size() != tids.size())
		{
			gcorr->addLandmarks(sids.toList().toVector(), tids.toList().toVector());
		}
		else
		{
			for (int i = 0; i < sids.size(); i++){
				gcorr->addLandmarks(QVector<QString>() << sids[i], QVector<QString>() << tids[i]);
			}
		}

		gcorr->computeCorrespondences();

		scheduler = QSharedPointer<Scheduler>(new Scheduler);
		blender = QSharedPointer<TopoBlender>(new TopoBlender(gcorr.data(), scheduler.data()));
		synthManager = QSharedPointer<SynthesisManager>(new SynthesisManager(gcorr.data(), scheduler.data(), blender.data(), numSamples));

		/// Reschedule:
		{
			QVector<QString> changingParts;

			// Move task of part to start
			for (auto n : scheduler->activeGraph->nodes){
				QString sid = n->id;
				if (sid == sourcePartName || source->shareGroup(sid, sourcePartName))
				{
					scheduler->moveTaskToStart(sid);
					changingParts << sid;
				}
				else
					n->property["skipSynth"].setValue(true);
			}

			// Move remaining to end
			int endTime = scheduler->endOf(QList<Task*>() << scheduler->getTaskFromNodeID(sourcePartName));
			scheduler->moveAllButTasksToTime(changingParts, endTime + 100);
		}

		/// Visualize schedule:
		if (false)
        {
            blender->parentWidget = new QMainWindow();
            blender->parentWidget->show();
            blender->setupUI();

            QStringList corr;
            for(auto n : scheduler->activeGraph->nodes){
                corr << QString("%1-%2").arg(n->id, n->property["correspond"].toString());
            }

            QMessageBox::information(blender->parentWidget, "Correspondence", corr.join("\n"));
        }

		/// Sample geometry
		synthManager->genSynData();

		/// Compute blending
		scheduler->property["forceStopTime"].setValue(0.5);
        scheduler->timeStep = 1.0 / 50.0;
		scheduler->executeAll();

		// Temporary solution for disconnection
		// TODO: use smarter relinking
		// Original center
		{
			auto m = document->getModel(sourceName);

			Vector3 oldCenter = m->getNode(sourcePartName)->center();

			bool isGroup = false;
			auto grp = m->groupsOf(sourcePartName);
			if (grp.size() && grp.front().size()) isGroup = true;
			if (isGroup) oldCenter = m->groupCenter(sourcePartName);

			Vector3 delta(0,0,0);

			for (auto g : scheduler->allGraphs)
			{
				Vector3 curCenter = g->getNode(sourcePartName)->center();
				if (isGroup) curCenter = g->groupCenter(sourcePartName);
				delta = oldCenter - curCenter;

				g->translate(delta, true);
			}

			// Make sure we have unique copies of the meshes
			for (auto n : scheduler->targetGraph->nodes){
				auto origMesh = scheduler->targetGraph->getMesh(n->id);
				auto newMesh = QSharedPointer<SurfaceMeshModel>(origMesh->clone());
				n->property["mesh"].setValue(newMesh);
			}
			scheduler->targetGraph->translate(delta, false);
		}
	}

	if (!synthManager.isNull() && !isBusy)
	{
		isBusy = true;

		double t = double(value) / 100.0;
		auto active_graph = scheduler->allGraphs[t * (scheduler->allGraphs.size() - 1)];
		auto cloud = synthManager->reconstructGeometryNode(active_graph->getNode(sourcePartName), t);

        // Blend the remaining elements of a group
        auto source = document->getModel(document->firstModelName());

        for(auto n : source->nodes){
            if(n->id == sourcePartName) continue;
            if(source->shareGroup(n->id, sourcePartName)){
				auto activeNode = active_graph->getNode(n->id);
				if (activeNode == nullptr) continue;

				auto pointsNormals = synthManager->reconstructGeometryNode(activeNode, t);
				cloud.first += pointsNormals.first;
				cloud.second += pointsNormals.second;
            }
        }

		emit(cloudReady(cloud));

		isBusy = false;
	}
}

void ManualBlendManager::finalizeBlend(QString source_part_name, QString target_name, QString target_part_name, int value)
{
    double t = double(value) / 100.0;

	auto model = document->getModel(document->firstModelName());
	auto active_graph = scheduler->allGraphs[t * (scheduler->allGraphs.size() - 1)];

    if(t == 0.0){
        SynthesisManager::OrientedCloud emptyCloud;
        emit(cloudReady(emptyCloud));
        return;
    }

	if (t == 1.0)
	{
		// Copy node and mesh geometry exactly from target	    
		{		
			QString tgnid = active_graph->getNode(source_part_name)->property["correspond"].toString();

			auto target_node = scheduler->targetGraph->getNode(tgnid);
			auto n = model->getNode(source_part_name);

			auto newNode = model->replaceNode(n->id, target_node->clone(), true);
			auto nodeMesh = scheduler->targetGraph->getMesh(target_node->id)->clone();
			nodeMesh->update_face_normals();
			nodeMesh->update_vertex_normals();
			nodeMesh->updateBoundingBox();

			newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
			newNode->id = source_part_name;

			// Select it
			model->activeNode = newNode;
		}

        // Blend the remaining elements of a group
		for (auto nj : model->nodes){
            if(nj->id == sourcePartName) continue;
			if (model->shareGroup(nj->id, sourcePartName)){
				QString tgnid = active_graph->getNode(nj->id)->property["correspond"].toString();

				auto target_node = scheduler->targetGraph->getNode(tgnid);
				if (target_node == nullptr) continue;

				auto node_id = nj->id;
				auto newNode = model->replaceNode(node_id, target_node->clone(), true);
				auto nodeMesh = scheduler->targetGraph->getMesh(target_node->id)->clone();
                nodeMesh->update_face_normals();
                nodeMesh->update_vertex_normals();
                nodeMesh->updateBoundingBox();

				newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
				newNode->id = node_id;
            }
        }

        SynthesisManager::OrientedCloud emptyCloud;
        emit(cloudReady(emptyCloud));

        return;
    }

    int reconLevel = 6;

    // Reconstruction Options
    switch(property("reconResolution").toInt()){
    case 0: reconLevel = 6; break;
    case 1: reconLevel = 7; break;
    case 2: reconLevel = 8; break;
    }

	// Collect group
	QSet<QString> groupIDs;
	groupIDs << sourcePartName;

	for (auto nj : model->nodes){
		if (model->shareGroup(nj->id, sourcePartName)){
			groupIDs << nj->id;
		}
	}

	for (auto nodeID : groupIDs)
	{
		auto n = model->getNode(nodeID);
		auto t_node = active_graph->getNode(nodeID);

		if (t_node == nullptr || n == nullptr) continue;

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
		PoissonRecon::makeFromCloud(finalP, finalN, mesh, reconLevel);

		// Copy results
		auto nodeMesh = QSharedPointer<SurfaceMeshModel>(new SurfaceMeshModel(nodeID));

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
		auto node_id = n->id;
		auto newNode = model->replaceNode(node_id, t_node->clone(), true);
		newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
		newNode->id = node_id;
	}

	model->deselectAll();
}
