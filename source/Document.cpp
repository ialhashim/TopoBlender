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
    models << model;
    return isLoaded;
}

void Document::createModel(QString modelName)
{
    auto model = QSharedPointer<Model>(new Model());
    model->loadFromFile(modelName);
    models << model;
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

void Document::modifyLastAdded(QString modelName, QVector<QVector3D> &guidePoints)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->modifyLastAdded(guidePoints);
}

void Document::generateSurface(QString modelName, double offset)
{
    auto m = getModel(modelName);
    if(m != nullptr) m->generateSurface(offset);
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

