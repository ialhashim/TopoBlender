#pragma once
#include <QGraphicsObject>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

class Document;

class Tool : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF bounds READ boundsRect WRITE setBounds)

public:
    Tool(Document * document);

    QRectF boundingRect() const { return childrenBoundingRect(); }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    QRectF bounds;
    QRectF boundsRect() { return bounds; }
    bool isShowBorder;

protected:
    Document * document;

public slots:
    void setBounds(const QRectF & newBounds);

signals:
    void boundsChanged();
};
