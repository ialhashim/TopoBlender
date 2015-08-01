#pragma once
#include <QGraphicsObject>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

class Tool : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF bounds READ boundsRect WRITE setBounds)

public:
    Tool();

    QRectF boundingRect() const { return childrenBoundingRect(); }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    QRectF bounds;
    QRectF boundsRect() { return bounds; }
    bool isShowBorder;

    QStringList messages;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);

public slots:
    void setBounds(const QRectF & newBounds);

signals:
    void boundsChanged();
};
