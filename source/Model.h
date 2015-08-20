#pragma once

#include <QObject>
#include <QMatrix4x4>
#include "ShapeGraph.h"

class Viewer;

class Model : public QObject, public Structure::ShapeGraph
{
    Q_OBJECT
public:
    explicit Model(QObject *parent = 0);

    void draw(Viewer * glwidget);

    void createCurveFromPoints(QVector<QVector3D> &points);
    void createSheetFromPoints(QVector<QVector3D> &points);

    void modifyLastAdded(QVector<QVector3D> &guidePoints);

    void generateSurface(double offset = 0.025);

    Structure::Node * activeNode;
	void storeActiveNodeGeometry();

public slots :
	void transformActiveNodeGeometry(QMatrix4x4 transform);
signals:
};
