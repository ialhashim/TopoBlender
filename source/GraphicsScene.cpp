#include "GraphicsScene.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QTimer>

GraphicsScene::GraphicsScene() : isGradientBackground(false)
{
    /// Background:
    // Colors
    auto lightBackColor = QColor::fromRgb(124, 143, 162);
    auto darkBackColor = QColor::fromRgb(27, 30, 32);

    // Gradient background
    if( isGradientBackground )
    {
        auto views = this->views();
        QLinearGradient gradient(0,0,0, views.size() ? views.front()->height() : 720);
        gradient.setColorAt(0.0, lightBackColor);
        gradient.setColorAt(1.0, darkBackColor);
        setBackgroundBrush(QBrush(gradient));
    }
    else
    {
        setBackgroundBrush(QBrush(darkBackColor));
    }
}

void GraphicsScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter,rect);
}

void GraphicsScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawForeground(painter,rect);
}

void GraphicsScene::wheelEvent(QGraphicsSceneWheelEvent *event)
{
	QGraphicsScene::wheelEvent(event);

	// block graphics view from receiving wheel events
	event->accept(); 
}

QGraphicsObject * GraphicsScene::getObjectByName(QString name)
{
    for(auto i: items()){
        auto obj = i->toGraphicsObject();
        if(!obj) continue;
        if(obj->objectName() == name){
            return obj;
        }
    }

    return nullptr;
}

void GraphicsScene::displayMessage(QString message, int time)
{
    auto textItem = addText(message, QFont("SansSerif", 24));
    textItem->setDefaultTextColor(Qt::white);

    QGraphicsDropShadowEffect * shadow = new QGraphicsDropShadowEffect();
    shadow->setColor(QColor(0,0,0,128));
    shadow->setOffset(3);
    shadow->setBlurRadius(4);
    textItem->setGraphicsEffect(shadow);
    auto textRect = textItem->sceneBoundingRect();
    textItem->moveBy(this->views().front()->width() * 0.5 - textRect.width() * 0.5,
                     this->views().front()->height() * 0.5 - textRect.height() * 0.5);
    textRect = textItem->sceneBoundingRect();
    textRect.adjust(-20,-20,20,20);

    QPainterPath textRectPath;
    textRectPath.addRoundedRect(textRect, 15, 15);
    auto rectItem = addPath(textRectPath, QPen(Qt::transparent), QBrush(QColor(0,0,0,200)));

    textItem->setZValue(500);
    rectItem->setZValue(499);

    QTimer::singleShot(time, [=]{
        removeItem(textItem);
        removeItem(rectItem);
        update();
    });
}

