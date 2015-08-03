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

    // Visualization:
    void drawModel(QString modelName, QWidget * widget);

    // Append to models:
    void createCurveFromPoints(QString modelName, QVector<QVector3D> &points);
    void createSheetFromPoints(QString modelName, QVector<QVector3D> &points);

    // Modify models:

    // Stats:
    QString firstModelName();

protected:
    QVector< QSharedPointer<Model> > models;
    Model * getModel(QString name);

signals:

public slots:
};

