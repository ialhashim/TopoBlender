#include "ResolveCorrespondence.h"

#include "ShapeGraph.h"

#include "GraphCorresponder.h"

ResolveCorrespondence::ResolveCorrespondence(Structure::Graph *shapeA,
                                             Structure::Graph *shapeB,
                                             QVector<QPair<QString, QString> > orig_pairs,
                                             GraphCorresponder *gcorr)
{
    auto groupsA = shapeA->groups;
    auto groupsB = shapeB->groups;

    auto isInGroup = [&](QString nodeID, QVector< QVector<QString> > groups){
        for(auto group : groups){
            if(group.contains(nodeID))
                return true;
        }
        return false;
    };

    // State
    QMap<QString, bool> matchedA, matchedB;
    for(auto n : shapeA->nodes) matchedA[n->id] = false;
    for(auto n : shapeB->nodes) matchedB[n->id] = false;

    // Matching as a map
    QMap<QString, QString> pairsA, pairsB;
    for(auto pair : orig_pairs)
    {
        pairsA[pair.first] = pair.second;
        pairsB[pair.second] = pair.first;
    }

    // Go over source nodes
    for(auto n : shapeA->nodes)
    {
        if(matchedA[n->id]) continue;

        // Check if it is matched
        if( pairsA.contains(n->id) )
        {
            QString match = pairsA[n->id];
            matchedA[n->id] = true;
            matchedB[match] = true;

            if(isInGroup(n->id, groupsA))
            // This is a many-
            {
                // All source many need to be matched
                QVector<QString> grpA;
				auto allGrpA = shapeA->groupsOf(n->id);
				for (auto j : allGrpA.front())
                    if(pairsA.contains(j))
                        grpA << j;

                if( isInGroup(match, groupsB) )
                // Many-to-many
                {
                    // Now we have a many-to-many that were matched
                    // Resolve all as a one-to-one (scheduler should fix timing..)
                    for(auto j : grpA)
                    {
                        gcorr->addLandmarks( QVector<QString>() << j, QVector<QString>() << pairsA[j] );
						matchedB[pairsA[j]] = true;
                    }
                }
                else
                // Many-to-one
                {
                    gcorr->addLandmarks( grpA, QVector<QString>() << match );
                }

                for(auto j : grpA) matchedA[j] = true;
            }
            else
            // This is a one-
            {
                if( isInGroup(match, groupsB) )
                // One-to-many
                {
                    // All target many need to be matched
                    QVector<QString> grpB;
					auto allGrpB = shapeB->groupsOf(match);
					for (auto j : allGrpB.front())
                        if(pairsB.contains(j))
                            grpB << j;

                    gcorr->addLandmarks( QVector<QString>() << n->id, grpB );
                    for(auto j : grpB) matchedB[j] = true;
                }
                else
                // One-to-one
                {
                    gcorr->addLandmarks( QVector<QString>() << n->id, QVector<QString>() << match );
                    matchedB[match] = true;
                }
            }
        }
    }

    // one-to-nothing
    for(auto nid : matchedA.keys()) if(!matchedA[nid]) gcorr->setNonCorresSource(nid);
    for(auto nid : matchedB.keys()) if(!matchedB[nid]) gcorr->setNonCorresTarget(nid);
}

