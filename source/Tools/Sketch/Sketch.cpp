#include <QGraphicsScene>

#include "Sketch.h"
#include "Viewer.h"

Sketch::Sketch(Document * document, const QRectF &bounds) : document(document)
{
    setBounds(bounds);

    connect(this, SIGNAL(boundsChanged()), SLOT(resizeViews()));

    setObjectName("sketch");
}

void Sketch::init()
{
    int numViews = 4;

    for(int i = 0; i < numViews; i++){
        auto type = SketchViewType(SketchViewType::VIEW_TOP + i);
        auto view = new SketchView(document, this, type);
        views << view;
    }
}

void Sketch::resizeViews()
{
    auto subdivide = [&](QRectF window, int count){
        QVector<QRectF> rects;
        auto hw = window.width() * 0.5;
        auto hh = window.height() * 0.5;
        if(count == 4)
        {
            rects << QRectF(0,0,hw,hh);
            rects << QRectF(hw,0,hw,hh);
            rects << QRectF(0,hh,hw,hh);
            rects << QRectF(hw,hh,hw,hh);
        }
        return rects;
    };

    QVector<QRectF> rects = subdivide(bounds, views.size());
    for(int i = 0; i < views.size(); i++){
        auto view = views[i];
        view->setPos(rects[i].topLeft());
        view->setRect(QRectF(0, 0, rects[i].width(), rects[i].height()));
    }
}
