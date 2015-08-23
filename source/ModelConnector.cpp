#include "ModelConnector.h"
#include "Model.h"

#define PQP_SUPPORT_SURFACEMESH
#include "PQP/PQPLib.h"

ModelConnector::ModelConnector(Model *g)
{
    QMap<QString, int> nodeID;

    PQP::Manager m(g->nodes.size());

    // load up meshes for all parts
    for(auto n : g->nodes)
    {
        nodeID[n->id] = nodeID.size();
        auto mesh = makeModelPQP(g->getMesh(n->id));
        if(mesh.empty()) continue;

        m.addModel(mesh);
    }

    QMap<QString, QVector<QPair<double, QString> > > possibleEdges;

    for(int i = 0; i < g->nodes.size(); i++)
    {
        for(int j = i+1; j < g->nodes.size(); j++)
        {
            auto ni = g->nodes[i];
            auto nj = g->nodes[j];

            // Ignore when edge existed
            if(g->getEdge(ni->id, nj->id)) continue;

            // Ignore when two nodes are in the same group
            if(g->shareGroup(ni->id, nj->id)) continue;

            // Check if edge needs to happen
            auto isects = m.testIntersection(i,j);
            std::sort(isects.begin(), isects.end());

            auto closest = isects.front();
            possibleEdges[ni->id].push_back( qMakePair(closest.distance, nj->id) );
            possibleEdges[nj->id].push_back( qMakePair(closest.distance, ni->id) );
        }
    }

    double threshold = g->robustBBox().diagonal().norm() * 0.05;

    for(auto nid : possibleEdges.keys())
    {
        auto possible = possibleEdges[nid];
        std::sort(possible.begin(), possible.end());

        for(auto edge : possible)
        {
            if(g->getEdge(nid, edge.second)) continue;

            double dist = edge.first;

            if(dist > threshold) continue;

            g->addEdge(g->getNode(nid), g->getNode(edge.second));

            //qDebug() << "Adding edge: " + nid + " - " + edge.second;
        }
    }
}

