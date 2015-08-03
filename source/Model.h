#pragma once

#include <QObject>

#include "ShapeGraph.h"
#include "Viewer.h"

class Model : public QObject, public Structure::ShapeGraph
{
    Q_OBJECT
public:
    explicit Model(QObject *parent = 0);

    void draw(Viewer * glwidget);

    void createCurveFromPoints(QVector<QVector3D> &points);
    void createSheetFromPoints(QVector<QVector3D> &points);

signals:

public slots:
};
