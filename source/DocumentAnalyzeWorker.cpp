#include "DocumentAnalyzeWorker.h"

#include <iostream>
#include <QThread>

#include "BatchProcess.h"
#include "ShapeGraph.h"

void DocumentAnalyzeWorker::process()
{
	QString sourceName = document->firstModelName();

    int loadShapesPercent = 10;
    int computeCorrespodPercent = 90;
	 
    // Get names of shapes of selected category into memory
    auto catModels = document->categories[ document->currentCategory ].toStringList();
	  
    // Load all shapes into memory
    for(int i = 0; i < catModels.size(); i++)
    {
        document->cacheModel(catModels.at(i));

        emit(progress(loadShapesPercent * (double(i) / (catModels.size()-1))));
    }

	// Check for stored correspondence results on disk
	QString matching_file = document->datasetPath + "/" + sourceName + "_matches.txt";
	if (QFileInfo(matching_file).exists()){
		document->loadDatasetCorr(matching_file);
		emit(progress(100));
		emit(finished());
		return;
	}

    // Compute correspondence with respect to active shape
    int k = 2; // search parameter

    auto cachedShapeA = document->getModel(sourceName);
    for(int i = 0; i < catModels.size(); i++)
    {
        QString targetName = catModels.at(i);

        if(sourceName == targetName) continue;

        auto cachedShapeB = document->cacheModel(targetName);

        QString sourceShape = "CACHED_" + sourceName;
        QString targetShape = "CACHED_" + targetName;

        QVariantMap options;
        options["roundtrip"].setValue(true);
        options["k"].setValue( k );
        options["isQuietMode"].setValue(true);
        options["isManyTypesJobs"].setValue(true);
        //options["align"].setValue(true);
        //options["isAllowCutsJoins"].setValue(true);
        //options["isIgnoreSymmetryGroups"].setValue(true);
        //options["isOutputMatching"].setValue(true);

        int numJobs = 0;

		QVector< QVector<QVariantMap> > reports;

		// Perform correspondence search
		{
			// First pass source to target
			{
				auto bp = QSharedPointer<BatchProcess>(new BatchProcess(sourceShape, targetShape, options));
				bp->cachedShapeA = QSharedPointer<Structure::ShapeGraph>(document->cloneAsShapeGraph(cachedShapeA));
				bp->cachedShapeB = QSharedPointer<Structure::ShapeGraph>(document->cloneAsShapeGraph(cachedShapeB));
				bp->jobUID = numJobs++;
				bp->run();
				reports << bp->jobReports;
			}

			// Second pass target to source
			if (options["roundtrip"].toBool())
			{
				auto bp2 = QSharedPointer<BatchProcess>(new BatchProcess(targetShape, sourceShape, options));
				bp2->cachedShapeA = QSharedPointer<Structure::ShapeGraph>(document->cloneAsShapeGraph(cachedShapeB));
				bp2->cachedShapeB = QSharedPointer<Structure::ShapeGraph>(document->cloneAsShapeGraph(cachedShapeA));
				bp2->jobUID = numJobs++;
				bp2->run();
				reports << bp2->jobReports;
			}
		}

        // Look at reports
        double minEnergy = 1.0;
        int totalTime = 0;
        QVariantMap minJob;
		for (auto & reportVec : reports){
			for (auto & report : reportVec){
				totalTime += report["search_time"].toInt();
                double c = report["min_cost"].toDouble();
                if (c < minEnergy){
                    minEnergy = c;
                    minJob = report;
                }
            }
        }

        //std::cout << "\nJobs computed: " << numJobs << "\n";
        //std::cout << minEnergy << " - " << qPrintable(minJob["img_file"].toString());

		auto firstReport = reports.front().front();

		for (auto p : minJob["matching_pairs"].value< QVector< QPair<QString, QString> > >())
		{
			// When best result was from the other way
			if (minJob["job_uid"].toInt() != firstReport["job_uid"].toInt()) std::swap(p.first, p.second);

			document->datasetCorr[p.first][targetName].push_back(p.second);
		}

        emit(progress(loadShapesPercent + (computeCorrespodPercent * (double(i) / (catModels.size()-1)))));
    }

	// Save results to disk
	document->saveDatasetCorr(matching_file);

    emit(finished());
}
