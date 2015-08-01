#pragma once
#include <QGraphicsScene>

class GraphicsScene : public QGraphicsScene
{
public:
    GraphicsScene();

    QGraphicsObject *getObjectByName(QString name);

protected:
    void drawBackground ( QPainter * painter, const QRectF & rect );
    void drawForeground ( QPainter * painter, const QRectF & rect );

    bool isGradientBackground;

	void wheelEvent(QGraphicsSceneWheelEvent *event);
};

