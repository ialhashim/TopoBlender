#pragma once
#include <QGraphicsObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QGraphicsSceneMouseEvent>

#include "SketchManipulatorTool.h"

class Document;

namespace Eigen{ class Camera; class Trackball; class Plane; }

enum SketchViewType{ VIEW_TOP, VIEW_FRONT, VIEW_LEFT, VIEW_CAMERA };
static QString SketchViewTypeNames[] = {"Top", "Front", "Left", "Camera"};

enum SketchViewOp{ SKETCH_NONE, SKETCH_CURVE, SKETCH_SHEET, DEFORM_SKETCH, TRANSFORM_PART };
static QString SketchViewOpName[] = {"Ready", "Sketch Curve", "Sketch Sheet", "Deform", "Transform Part"};

class SketchView : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    SketchView(Document * document, QGraphicsItem * parent, SketchViewType type);
	~SketchView();
    SketchViewType type;

    Document * document;

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
	void prePaint(QPainter * painter, QWidget * widget);
	void postPaint(QPainter * painter, QWidget * widget);

    QRectF rect;
    QRectF boundingRect() const { return this->rect; }
    void setRect(const QRectF & newRect){ this->rect = newRect; }

    // Camera movement
    Eigen::Camera* camera;
    Eigen::Trackball* trackball;

    // Camera properties
    QMatrix4x4 projectionMatrix, viewMatrix;
    QVector3D eyePos;

    static QMatrix4x4 defaultOrthoViewMatrix(QVector3D &eye, int type, int w, int h, QVector3D translation,
                                             float zoomFactor, QMatrix4x4 & proj, QMatrix4x4 & view);

    QVector3D screenToWorld(QVector3D point2D);
    QVector3D screenToWorld(QPointF point2D);
    QVector3D worldToScreen(QVector3D point3D);

    // Sketching stuff
    Eigen::Plane * sketchPlane;
	QVector<QVector3D> sketchPoints;
	SketchViewOp sketchOp;

	SketchManipulatorTool * manTool;

    QStringList messages;

public slots:
	void setSketchOp(SketchViewOp toSketchOp);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
	void wheelEvent(QGraphicsSceneWheelEvent *);
	QPointF buttonDownCursorPos, mouseMoveCursorPos, buttonUpCursorPos;
	bool leftButtonDown, rightButtonDown, middleButtonDown;
};
