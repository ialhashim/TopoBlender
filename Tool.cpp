#include "Tool.h"
#include <QGraphicsScene>

Tool::Tool() : isShowBorder(false)
{
    this->setAcceptHoverEvents(true);
    //this->setFlags( QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable );
}

void Tool::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *){

    if(isShowBorder){
        painter->drawRect(bounds);
    }
    painter->setPen(QPen(Qt::white,10));

    int x = 15, y = 45;
    for(auto message : messages){
        painter->drawText(x,y,message);
        y += 12;
    }
}

void Tool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseMoveEvent(event);

    messages.clear();
    messages << QString("mouse pos: %1, %2").arg(event->pos().x()).arg(event->pos().y());
}

void Tool::setBounds(const QRectF &newBounds){
    this->bounds = newBounds;

    if(!scene()) return;
    int delta = 0;
    for(auto i: scene()->items()){
        auto obj = i->toGraphicsObject();
        if(!obj) continue;
        if(obj->objectName() == "modifiersWidget"){
            delta = obj->boundingRect().width();
            break;
        }
    }

    bounds.adjust(0, 0, -delta, 0);

    emit(boundsChanged());
}
