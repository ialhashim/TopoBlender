#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include "ui_Sketch.h"

#include "Sketch.h"
#include "Viewer.h"
#include "Document.h"

Sketch::Sketch(Document * document, const QRectF &bounds) : document(document)
{
    setBounds(bounds);

    connect(this, SIGNAL(boundsChanged()), SLOT(resizeViews()));

    setObjectName("sketch");
}

void Sketch::init()
{
    // Add four views
    int numViews = 4;
    for(int i = 0; i < numViews; i++){
        auto type = SketchViewType(SketchViewType::VIEW_TOP + i);
        auto view = new SketchView(document, this, type);
        views << view;
    }

    // Add sketching UI elements
    auto toolsWidgetContainer = new QWidget();
    toolsWidget = new Ui::SketchToolsWidget();
    toolsWidget->setupUi(toolsWidgetContainer);
    auto toolsWidgetProxy = scene()->addWidget(toolsWidgetContainer);

    // Connect UI with sketch views
    {
        // Sketching new parts
        {
            connect(toolsWidget->curveButton, &QPushButton::pressed, [&](){
                for(auto & v : views) v->sketchOp = SKETCH_CURVE;
            });

            connect(toolsWidget->sheetButton, &QPushButton::pressed, [&](){
                for(auto & v : views) v->sketchOp = SKETCH_SHEET;
            });
        }

        // Deforming existing parts
        {
            connect(toolsWidget->deformButton, &QPushButton::pressed, [&](){
                for(auto & v : views) v->sketchOp = DEFORM_SKETCH;
            });
        }

        // Surface
        {
            connect(toolsWidget->surfaceButton, &QPushButton::pressed, [&](){
                document->generateSurface(document->firstModelName(), 0.03);
            });
        }

		// Save shape
		{
			connect(toolsWidget->saveButton, &QPushButton::pressed, [&](){
				document->saveModel(document->firstModelName());
			});
		}
    }

    // Place at bottom left corner
    auto delta = toolsWidgetProxy->sceneBoundingRect().bottomLeft() -
            scene()->views().front()->rect().bottomLeft();
    toolsWidgetProxy->moveBy(-delta.x(), -delta.y());
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
