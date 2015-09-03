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
    int numSamples = 2500;

    // Reconstruction Options
    switch(property("reconResolution").toInt()){
    case 0: numSamples = 2500; break;
    case 1: numSamples = 8000; break;
    case 2: numSamples = 20000; break;
    }

	if (sourcePartName != source_part_name || targetName != target_name || targetPartName != target_part_name)
	{
		this->sourcePartName = source_part_name;
		this->targetName = target_name;
		this->targetPartName = target_part_name;

        QString sourceName = document->firstModelName();

        auto source = document->getModel(sourceName);
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
                if (document->datasetCorr[sourceName][sid][targetName].empty())
				{
					gcorr->setNonCorresSource(sid);
				}
				else
				{
                    auto tid = document->datasetCorr[sourceName][sid][targetName].front();
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

        // Blend the remaining elements of a group
        auto source = document->getModel(document->firstModelName());

        for(auto n : source->nodes){
            if(n->id == sourcePartName) continue;
            if(source->shareGroup(n->id, sourcePartName)){
                auto tnode = t_graph->getNode(n->id);
                if(tnode == nullptr) continue;

                auto c = synthManager->reconstructGeometryNode(tnode, t);
                cloud.first += c.first;
                cloud.second += c.second;
            }
        }
		
		emit(cloudReady(cloud));

		isBusy = false;
	}
}

void ManualBlendManager::finalizeBlend(QString source_part_name, QString target_name, QString target_part_name, int value)
{
    double t = double(value) / 100.0;
    auto t_graph = scheduler->allGraphs[t * (scheduler->allGraphs.size() - 1)];
    auto t_node = t_graph->getNode(source_part_name);

    auto model = document->getModel(document->firstModelName());
    auto n = model->getNode(source_part_name);

    if(t == 0.0){
        SynthesisManager::OrientedCloud emptyCloud;
        emit(cloudReady(emptyCloud));
        return;
    }

    if(t == 1.0){
        // Copy node and mesh geometry exactly from target
		QString tgnid = t_node->property["correspond"].toString();
        t_node = scheduler->targetGraph->getNode(tgnid);

        auto newNode = model->replaceNode(n->id, t_node->clone(), true);
		auto nodeMesh = scheduler->targetGraph->getMesh(t_node->id)->clone();
        nodeMesh->update_face_normals();
        nodeMesh->update_vertex_normals();
        nodeMesh->updateBoundingBox();
        newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
        model->activeNode = newNode;

        // Blend the remaining elements of a group
        auto source = model;
        for(auto nj : source->nodes){
            if(nj->id == sourcePartName) continue;
            if(source->shareGroup(nj->id, sourcePartName)){
                t_node = t_graph->getNode(nj->id);
                if(t_node == nullptr) continue;

                // Copy node and mesh geometry exactly from target
                QString tgnid = t_node->property["correspond"].toString();
                t_node = scheduler->targetGraph->getNode(tgnid);

                auto newNode = model->replaceNode(nj->id, t_node->clone(), true);
                auto nodeMesh = scheduler->targetGraph->getMesh(t_node->id)->clone();
                nodeMesh->update_face_normals();
                nodeMesh->update_vertex_normals();
                nodeMesh->updateBoundingBox();
                newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
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
    auto newNode = model->replaceNode(n->id, t_node->clone(), true);
    newNode->property["mesh"].setValue(QSharedPointer<SurfaceMeshModel>(nodeMesh));
    model->activeNode = newNode;
}
