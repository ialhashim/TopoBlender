#include "Document.h"
#include "Model.h"
#include "Viewer.h"

Document::Document(QObject *parent) : QObject(parent)
{

}

bool Document::loadModel(QString filename)
{
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

QString Document::firstModelName()
{
    if(models.isEmpty()) return "";
    return models.front()->name();
}

Model* Document::getModel(QString name)
{
    for(auto m : models) if(m->name() == name) return m.data();
    return nullptr;
}

