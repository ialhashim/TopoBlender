#pragma once
#include <QGraphicsScene>

class QGraphicsObject;

class GraphicsScene : public QGraphicsScene
{
public:
    GraphicsScene();

    QGraphicsObject *getObjectByName(QString name);

    void displayMessage(QString message, int time = 3000);

protected:
    void drawBackground ( QPainter * painter, const QRectF & rect );
    void drawForeground ( QPainter * painter, const QRectF & rect );

    bool isGradientBackground;

    void wheelEvent(QGraphicsSceneWheelEvent *event);
};

