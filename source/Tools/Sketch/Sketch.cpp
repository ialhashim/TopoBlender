#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QFileDialog>
#include "ui_Sketch.h"

#include "Sketch.h"
#include "Viewer.h"
#include "Document.h"

#include "SketchDuplicate.h"
#include "ui_SketchDuplicate.h"

#include "ModelConnector.h"

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

    // Place at bottom left corner
    auto delta = toolsWidgetProxy->sceneBoundingRect().bottomLeft() -
            scene()->views().front()->rect().bottomLeft();
    toolsWidgetProxy->moveBy(-delta.x(), -delta.y());

    // Add duplication tool
    dupToolWidget = new SketchDuplicate();
    dupToolWidgetProxy = scene()->addWidget(dupToolWidget);
    auto dupDelta = toolsWidgetProxy->pos() + QPointF(toolsWidgetProxy->size().width(),0);
    dupToolWidgetProxy->moveBy(dupDelta.x(), dupDelta.y());
    dupToolWidgetProxy->setVisible(false);
    dupToolWidgetProxy->setFlags(QGraphicsItem::ItemIsMovable);
    //dupToolWidgetProxy->setWindowFlags(Qt::Window);

    // Connect UI with sketch views
    {
        // Sketching new parts
        {
            connect(toolsWidget->curveButton, &QPushButton::pressed, [&](){
                for(auto & v : views) v->setSketchOp(SKETCH_CURVE);
            });

            connect(toolsWidget->sheetButton, &QPushButton::pressed, [&](){
				for (auto & v : views) v->setSketchOp(SKETCH_SHEET);
            });
        }

        // Modify
        {
            // Select parts
            connect(toolsWidget->selectButton, &QPushButton::pressed, [&](){
                for (auto & v : views) v->setSketchOp(SELECT_PART);
            });

            // Deform parts
            connect(toolsWidget->deformButton, &QPushButton::pressed, [&](){
				for (auto & v : views) v->setSketchOp(DEFORM_SKETCH);
            });

            // Transform parts
            connect(toolsWidget->transformButton, &QPushButton::pressed, [&](){
                for (auto & v : views) v->setSketchOp(TRANSFORM_PART);
            });

            // Normalize and place on ground
            connect(toolsWidget->placeGroundButton, &QPushButton::pressed, [&](){
                document->placeOnGround(document->firstModelName());
            });

            // Find all possible edges
            connect(toolsWidget->edgesButton, &QPushButton::pressed, [&](){
                ModelConnector m(document->getModel(document->firstModelName()));
            });
        }

        // IO
        {
            connect(toolsWidget->loadButton, &QPushButton::pressed, [&](){
                QString filename = QFileDialog::getOpenFileName(0, "Load shape", "", "Shape graph (*.xml)");

                if(filename.size()){
                    document->clearModels();
                    document->loadModel(filename);
                }
            });
			connect(toolsWidget->saveButton, &QPushButton::pressed, [&](){
                QString filename = QFileDialog::getSaveFileName(0, "Save shape", "", "Shape graph (*.xml)");

                document->saveModel(document->firstModelName(), filename);
            });
		}
    }

    // Duplicate tool UI
    {
        connect(toolsWidget->replicateButton, &QPushButton::toggled, [&](bool checked){
            if(checked){
                dupToolWidgetProxy->setVisible(true);
                emit(dupToolWidget->settingsChanged());
            }else{
                dupToolWidgetProxy->setVisible(false);
            }
        });

        connect(dupToolWidget, &SketchDuplicate::settingsChanged, [&](){
            QString dupOperation = dupToolWidget->dupOperation();
            document->duplicateActiveNodeViz(document->firstModelName(), dupOperation);

            scene()->update();
        });

        connect(dupToolWidget, &SketchDuplicate::acceptedOperation, [&](){
            dupToolWidgetProxy->setVisible(false);

            QString dupOperation = dupToolWidget->dupOperation();
            document->duplicateActiveNode(document->firstModelName(), dupOperation);

            toolsWidget->replicateButton->setChecked(false);

            scene()->update();
        });
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
