#include "GraphicsView.h"
#include <QGraphicsProxyWidget>

GraphicsView::GraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    this->setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
    this->setVerticalScrollBarPolicy ( Qt::ScrollBarAlwaysOff );
}

void GraphicsView::resizeEvent(QResizeEvent *event){
    emit(resized(QRectF(rect())));
    QGraphicsView::resizeEvent(event);
}
