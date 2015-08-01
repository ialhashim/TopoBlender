#pragma once
#include <QGraphicsObject>
#include <QMatrix4x4>
#include <QGraphicsSceneMouseEvent>

namespace Eigen{ class Camera; class Trackball; }

enum SketchViewType{ VIEW_TOP, VIEW_FRONT, VIEW_LEFT, VIEW_CAMERA };
static QString SketchViewTypeNames[] = {"Top", "Front", "Left", "Camera"};

class SketchView : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    SketchView(QGraphicsItem * parent, SketchViewType type);
	~SketchView();
    SketchViewType type;

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
	void prePaint(QPainter * painter, QWidget * widget);
	void postPaint(QPainter * painter, QWidget * widget);

    QRectF rect;
    QRectF boundingRect() const { return this->rect; }
    void setRect(const QRectF & newRect){ this->rect = newRect; }

    QStringList messages;

    Eigen::Camera* camera;
	Eigen::Trackball* trackball;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
	void wheelEvent(QGraphicsSceneWheelEvent *);
	QPointF buttonDownCursorPos, mouseMoveCursorPos, buttonUpCursorPos;
	bool leftButtonDown, rightButtonDown, middleButtonDown;
};
