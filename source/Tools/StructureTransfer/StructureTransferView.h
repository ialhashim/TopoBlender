#pragma once

#include <QGraphicsObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <Eigen/Core>

class Document;
class Gallery;
class Thumbnail;

namespace Eigen{ class Camera; class Trackball; class Plane; }

class StructureTransferView : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    StructureTransferView(Document * document, QGraphicsItem *parent);
    ~StructureTransferView();

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
    void prePaint(QPainter * painter, QWidget * widget);
    void postPaint(QPainter * painter, QWidget * widget);

    QRectF rect;
    QRectF boundingRect() const { return this->rect; }
    void setRect(const QRectF & newRect){ this->rect = newRect; }

protected:
    Document * document;

public:
    // Camera movement
    Eigen::Camera* camera;
    Eigen::Trackball* trackball;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
    void wheelEvent(QGraphicsSceneWheelEvent *);
    QPointF buttonDownCursorPos, mouseMoveCursorPos, buttonUpCursorPos;
    bool leftButtonDown, rightButtonDown, middleButtonDown;

    void keyPressEvent(QKeyEvent * event);

signals:
};
