#include <random>
#include <algorithm>

#include "Document.h"
#include "Model.h"
#include "Viewer.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QProgressDialog>
#include <QTimer>
#include <QThread>

#include "DocumentAnalyzeWorker.h"

Document::Document(QObject *parent) : QObject(parent)
{

}

bool Document::loadModel(QString filename)
{
    if(!QFileInfo(filename).exists()) return false;

    auto model = QSharedPointer<Model>(new Model());
    bool isLoaded = model->loadFromFile(filename);
    models.push_front(model);
    return isLoaded;
}

void Document::createModel(QString modelName)
{
    auto model = QSharedPointer<Model>(new Model());
    model->loadFromFile(modelName);
    models << model;
}

void Document::saveModel(QString modelName, QString filename)
{
    auto m = getModel(modelName);
    if (m != nullptr){
        Structure::ShapeGraph * model = m;
        if(filename.size() < 3)
            filename = model->property.contains("name") ? model->property["name"].toString() : QString("%1.xml").arg(modelName);
        m->saveToFile(filename);
    }
}

void Document::clearModels()
{
    models.clear();
}

bool Document::loadDataset(QString datasetFolder)
{
    if(datasetFolder.trimmed().size() == 0) return false;

    // Store dataset path
    datasetPath = datasetFolder;

    // Load shapes in dataset
    {
        QDir datasetDir(datasetPath);
        QStringList subdirs = datasetDir.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);

        for(auto subdir : subdirs)
        {
            // Special folders
            if (subdir == "corr") continue;

            QDir d(datasetPath + "/" + subdir);

            // Check if no graph is in this folder
            if (d.entryList(QStringList() << "*.xml", QDir::Files).isEmpty()) continue;

            auto xml_files = d.entryList(QStringList() << "*.xml", QDir::Files);

            if(xml_files.empty()) continue;

            dataset[subdir]["Name"] = subdir;
            dataset[subdir]["graphFile"] = d.absolutePath() + "/" + (xml_files.front());
            dataset[subdir]["thumbFile"] = d.absolutePath() + "/" + d.entryList(QStringList() << "*.png", QDir::Files).join("");
            dataset[subdir]["objFile"] = d.absolutePath() + "/" + d.entryList(QStringList() << "*.obj", QDir::Files).join("");
        }
    }

    // Load categories
    if(!dataset.empty())
    {
        auto existShapes = dataset.keys();

        QFile file( datasetPath + "/categories.txt" );
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            while (!in.atEnd()){
                QStringList line = in.readLine().split("|");
                if(line.size() < 2) continue;

                QString catName = line.front().trimmed();
                QStringList catElements = line.back().split(QRegExp("[ \t]"), QString::SkipEmptyParts);

                // Check they actually exist before adding them
                QStringList existingElements;
                for(auto shape : catElements){
                    if(existShapes.contains(shape))
                        existingElements << shape;
                }

                if(!existingElements.empty())
                    categories[catName] = existingElements;
            }

			if (!categories.empty())
				currentCategory = categories.keys().front();
        }
        else
        {
            // Assume all are of the same category
            currentCategory = "shapes";
            QVariant catElements;
            QStringList elements = QStringList::fromVector(dataset.keys().toVector());
            catElements.setValue(elements);
            categories.insert(currentCategory, catElements);
        }
    }

    return !dataset.empty();
}

QString Document::categoryOf(QString modelName)
{
    QString result;

    auto m = getModel(modelName);
    if(m == nullptr) return result;

    QString modelCategoryName = QFileInfo(((Structure::ShapeGraph*)m)->property["name"].toString()).dir().dirName();

    for(auto cat : categories.keys()){
        for(auto mname : categories[cat].toStringList()){
            if(mname == modelCategoryName)
                return cat;
        }
    }

    return result;
}

void Document::analyze(QString categoryName)
{
    if(!categories.keys().contains(categoryName)) return;

    // Create progress bar
    auto bar = new QProgressDialog("Please wait...", QString(), 0, 100, 0);
    bar->setWindowTitle("Processing");
    bar->show();
    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = (screenGeometry.width()-bar->width()) / 2;
    int y = (screenGeometry.height()-bar->height()) / 2;
    bar->move(x, y);

    auto worker = new DocumentAnalyzeWorker(this);

    QThread* thread = new QThread;
    worker->moveToThread(thread);
    connect(thread, SIGNAL (started()), worker, SLOT (processShapeDataset()));
    connect(worker, SIGNAL (finished()), thread, SLOT (quit()));
    connect(worker, SIGNAL (finished()), this, SLOT (sayCategoryAnalysisDone()));
    connect(worker, SIGNAL (finished()), bar, SLOT (deleteLater()));
    connect(worker, SIGNAL (finished()), worker, SLOT (deleteLater()));
    connect(thread, SIGNAL (finished()), thread, SLOT (deleteLater()));

    // Connect progress bar to worker
    bar->connect(worker, SIGNAL(progress(int)), SLOT(setValue(int)));
    bar->connect(worker, SIGNAL(progressText(QString)), SLOT(setLabelText(QString)));

    thread->start(QThread::HighestPriority);
}

void Document::computePairwise(QString categoryName)
{
    if(!categories.keys().contains(categoryName)) return;

    // Create progress bar
    auto bar = new QProgressDialog("Please wait...", QString(), 0, 100, 0);
    bar->setWindowTitle("Processing");
    bar->show();
    QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = (screenGeometry.width()-bar->width()) / 2;
    int y = (screenGeometry.height()-bar->height()) / 2;
    bar->move(x, y);

    auto worker = new DocumentAnalyzeWorker(this);

    QThread* thread = new QThread;

    worker->moveToThread(thread);
    connect(thread, SIGNAL (started()), worker, SLOT (processAllPairWise()));
    connect(worker, SIGNAL (finished()), thread, SLOT (quit()));
    connect(worker, SIGNAL (finished()), this, SLOT (sayPairwiseAnalysisDone()));
    connect(worker, SIGNAL (finished()), bar, SLOT (deleteLater()));
    connect(worker, SIGNAL (finished()), worker, SLOT (deleteLater()));
    connect(thread, SIGNAL (finished()), thread, SLOT (deleteLater()));

    // Connect progress bar to worker
    bar->connect(worker, SIGNAL(progress(int)), SLOT(setValue(int)));
    bar->connect(worker, SIGNAL(progressText(QString)), SLOT(setLabelText(QString)));

    thread->start(QThread::HighestPriority);
}

void Document::sayCategoryAnalysisDone()
{
    emit(categoryAnalysisDone());
}

void Document::sayPairwiseAnalysisDone()
{
    emit(categoryPairwiseDone());
}

void Document::sayGlobalSettingsChanged()
{
    emit(globalSettingsChanged());
}

void Document::savePairwise(QString filename)
{
	// Pair-wise distances
	{
		QFile file(filename);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
		QTextStream out(&file);
		for (auto s : datasetMatching.keys())
			for (auto t : datasetMatching[s].keys())
				out << s << " " << t << " " << datasetMatching[s][t]["min_cost"].toDouble() << "\n";
	}

	// Full dataset matches
	{
		QFile file(filename + ".match");
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
		QTextStream out(&file);
		for (auto s : datasetMatching.keys()){
			for (auto t : datasetMatching[s].keys()){
				QStringList matches;
				QVector< QPair<QString, QString> > mp = datasetMatching[s][t]["matching_pairs"].value< QVector< QPair<QString, QString> > >();
                for (auto p : mp)
                {
                    if(datasetMatching[s][t]["isReversed"].toBool()) std::swap(p.first, p.second);

					matches << QString("%1,%2").arg(p.first).arg(p.second);

					datasetCorr[s][p.first][t] << p.second;
					datasetCorr[t][p.second][s] << p.first;
				}
				out << s << " " << t << " " << matches.join("|") << "\n";
			}
		}
	}
}

void Document::loadPairwise(QString filename)
{
	// Pair-wise distances
	{
		QFile file(filename);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

		QTextStream in(&file);
		auto lines = in.readAll().split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

		for (auto line : lines){
			auto item = line.split(QRegExp("[ \t]"), QString::SkipEmptyParts);
			if (item.size() != 3) continue;
			datasetMatching[item[0]][item[1]]["min_cost"] = item[2].toDouble();
			datasetMatching[item[1]][item[0]]["min_cost"] = item[2].toDouble();
		}
	}
    
	// Full dataset matches
	{
		QFile file(filename + ".match");
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

		QTextStream in(&file);
		auto lines = in.readAll().split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

		for (auto line : lines){
			auto l = line.split(" ", QString::SkipEmptyParts);
			auto s = l.at(0);
			auto t = l.at(1);
			auto matchesPart = l.at(2).split("|", QString::SkipEmptyParts);

			for (auto txtPair : matchesPart)
			{
				auto p = txtPair.split(",", QString::SkipEmptyParts);

				datasetCorr[s][p.front()][t] << p.back();
				datasetCorr[t][p.back()][s] << p.front();
			}
		}
	}

    // Load clustering file if any
    {
        QFile file(filename + ".cluster");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QTextStream in(&file);
        auto lines = in.readAll().split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

        QVector<QColor> cluster_colors;

        // Intresting colors
        /*
        auto paired_colors = { "#a6cee3", "#1f78b4", "#b2df8a", "#33a02c", "#fb9a99", "#e31a1c",
                    "#fdbf6f", "#ff7f00", "#cab2d6", "#6a3d9a", "#ffff99", "#b15928",
                    "#4D4D4D", "#5DA5DA", "#FAA43A", "#60BD68", "#F17CB0", "#B2912F", "#B276B2", "#DECF3F", "#F15854",
                    "#e41a1c", "#377eb8", "#4daf4a", "#984ea3", "#ff7f00", "#ffff33", "#a65628", "#f781bf" };
        int num_paired_colors = paired_colors.size();
        for (int i = 0; i < num_paired_colors; i++){
            QColor c;
            c.setNamedColor(*(paired_colors.begin() + i));
            cluster_colors << c;
        }*/


        cluster_colors << QColor(255, 97, 121) << QColor(255, 219, 88) << QColor(107, 255, 135) << QColor(255, 165, 107) << QColor(104, 126, 255) <<
                        QColor(242, 5, 135) << QColor(138, 0, 242) << QColor(3, 166, 60) << QColor(242, 203, 5);


        // Generate random colors
        srand(time(0));
        for(int c = 0; c < lines.size(); c++) cluster_colors << starlab::qRandomColor2();

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(cluster_colors.begin(), cluster_colors.end(), g);

        for (int i = 0; i < lines.size(); i++)
        {
            auto line = lines[i];

            auto l = line.split("\t", QString::SkipEmptyParts);

            for(auto shapePart : l)
            {
                auto p = shapePart.split(":");
                cacheModel(p.front())->setColorFor(p.back(), cluster_colors[i]);
            }
        }
    }
}

void Document::saveDatasetCorr(QString filename)
{
    QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

	QTextStream out(&file);

    for (auto s : datasetCorr.keys())
        for (auto p : datasetCorr[s].keys())
            for (auto t : datasetCorr[s][p].keys())
                for (auto q : datasetCorr[s][p][t])
                    out << s << " " << p << " " << t << " " << q << "\n";
}

void Document::loadDatasetCorr(QString filename)
{
    QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

	QTextStream in(&file);
	auto lines = in.readAll().split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

	for (auto line : lines){
		auto item = line.split(QRegExp("[ \t]"), QString::SkipEmptyParts);
        if (item.size() != 4) continue;
        datasetCorr[item[0]][item[1]][item[2]].push_back(item[3]);
    }
}

void Document::drawModel(QString name, QWidget *widget)
{
    auto glwidget = (Viewer*)widget;
    if (glwidget == nullptr) return;

    auto m = getModel(name);
    if(m != nullptr) m->draw(glwidget);
}

void Document::createCurveFromPoints(QString modelName, QVector<QVector3D> & points)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->createCurveFromPoints(points);
}

void Document::createSheetFromPoints(QString modelName, QVector<QVector3D> & points)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->createSheetFromPoints(points);
}

void Document::duplicateActiveNodeViz(QString modelName, QString duplicationOp)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->duplicateActiveNodeViz(duplicationOp);
}

void Document::duplicateActiveNode(QString modelName, QString duplicationOp)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->duplicateActiveNode(duplicationOp);
}

void Document::modifyActiveNode(QString modelName, QVector<QVector3D> &guidePoints)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->modifyLastAdded(guidePoints);
}

void Document::setModelProperty(QString modelName, QString propertyName, QVariant propertyValue)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->setProperty(propertyName.toLatin1(), propertyValue);
}

void Document::generateSurface(QString modelName, double offset)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->generateSurface(offset);
}

void Document::placeOnGround(QString modelName)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->placeOnGround();
}

void Document::selectPart(QString modelName, QVector3D rayOrigin, QVector3D rayDirection)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->selectPart(rayOrigin, rayDirection);
}

void Document::deselectAll(QString modelName)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->deselectAll();
}

QVector3D Document::centerActiveNode(QString modelName)
{
    auto m = getModel(modelName);
    if(m == nullptr) return QVector3D(0,0,0);

    auto c = m->activeNode->center();
    return QVector3D(c[0],c[1],c[2]);
}

void Document::removeActiveNode(QString modelName)
{
    auto m = getModel(modelName);
    if(m == nullptr) return;

    QString nid = m->activeNode->id;
    m->removeNode(nid);
    m->activeNode = nullptr;
}

QString Document::firstModelName()
{
    if(models.isEmpty()) return "";
    return models.front()->name();
}

QVector3D Document::extent()
{
    QVector3D zero(0,0,0);
    if(models.isEmpty()) return zero;
    auto m = models.front();
    if(m->nodes.empty()) return zero;
    auto diag = m->robustBBox().diagonal();
    return QVector3D(diag.x(), diag.y(), diag.z());
}

Model* Document::getModel(QString name)
{
    for(auto m : models) if(m->name() == name) return m.data();
    return nullptr;
}

Model* Document::cacheModel(QString name)
{
    if(cachedModels.contains(name)){
        return cachedModels[name].data();
    }else{
        if(!dataset.contains(name)) return nullptr;
        QString filename = dataset[name]["graphFile"].toString();

        auto model = QSharedPointer<Model>(new Model());
        if(!model->loadFromFile(filename)) return nullptr;
        cachedModels[name] = model;
        return model.data();
    }
}

Structure::ShapeGraph * Document::cloneAsShapeGraph(Model * m)
{
	return m->cloneAsShapeGraph();
}
