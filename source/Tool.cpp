#include "Tool.h"
#include <QGraphicsScene>

Tool::Tool(Document *document) : document(document), isShowBorder(false)
{
    this->setAcceptHoverEvents(true);
    //this->setFlags( QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable );
}

void Tool::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if(isShowBorder){
        painter->setPen(QPen(Qt::red, 5));
        painter->drawRect(bounds);
    }
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
