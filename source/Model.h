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

    void duplicateActiveNodeViz(QString duplicationOp);
    void duplicateActiveNode(QString duplicationOp);

    void modifyLastAdded(QVector<QVector3D> &guidePoints);

    void generateSurface(double offset = 0.025);

    void placeOnGround();

    void selectPart(QVector3D rayOrigin, QVector3D rayDirection);
    void deselectAll();

    Structure::Node * activeNode;
	void storeActiveNodeGeometry();

    QVector< QSharedPointer<Structure::Node> > tempNodes;

    Structure::ShapeGraph * cloneAsShapeGraph();

	QString name();

protected:
    QVector< Structure::Node* > makeDuplicates(Structure::Node* n, QString duplicationOp);

public slots :
	void transformActiveNodeGeometry(QMatrix4x4 transform);
signals:
};
