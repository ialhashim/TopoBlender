#pragma once

#include <QObject>
#include <QVector>
#include <QSharedPointer>

class Model;

class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject *parent = 0);

    // IO:
    bool loadModel(QString filename);
    void createModel(QString modelName);

    // Visualization:
    void drawModel(QString modelName, QWidget * widget);

    // Append to models:
    void createCurveFromPoints(QString modelName, QVector<QVector3D> &points);
    void createSheetFromPoints(QString modelName, QVector<QVector3D> &points);

    // Modify models:
    void modifyLastAdded(QString modelName, QVector<QVector3D> &guidePoints);
    void generateSurface(QString modelName, double offset);

    // Stats:
    QString firstModelName();

protected:
    QVector< QSharedPointer<Model> > models;
    Model * getModel(QString name);

signals:

public slots:
};

