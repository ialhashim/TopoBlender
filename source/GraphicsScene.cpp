#include "GraphicsScene.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>

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

