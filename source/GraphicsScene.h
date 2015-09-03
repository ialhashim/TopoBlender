#pragma once
#include <QGraphicsScene>

class QGraphicsObject;

class GraphicsScene : public QGraphicsScene
{
public:
    GraphicsScene();

    QGraphicsObject *getObjectByName(QString name);

    void displayMessage(QString message, int time = 3000);

    void showPopup(QString message);
    void hidePopup();

protected:
    void drawBackground ( QPainter * painter, const QRectF & rect );
    void drawForeground ( QPainter * painter, const QRectF & rect );

    bool isGradientBackground;

    void wheelEvent(QGraphicsSceneWheelEvent *event);

    QGraphicsTextItem * popup;
};

