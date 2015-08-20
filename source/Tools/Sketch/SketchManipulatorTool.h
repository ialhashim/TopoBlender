#pragma once
#include <QGraphicsObject>
#include <QMatrix4x4>

class Model;

class SketchManipulatorTool : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)
public:
    SketchManipulatorTool(QGraphicsItem *parent = 0);

    QRectF boundingRect() const { return rect; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    QRectF rect;

    enum ManipulationOp{TRANSLATE, ROTATE, SCALE_X, SCALE_Y, SCALE_XY} manOp;

    QMatrix4x4 transform;

	Model * model;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

    QPointF buttonDownCursorPos, mouseMoveCursorPos, buttonUpCursorPos;
    bool leftButtonDown, rightButtonDown, middleButtonDown;
    QPointF startPos;

    QStringList messages;

public slots:
    void setRect(const QRectF & newRect);

signals:
    void transformChange(QMatrix4x4);
};
