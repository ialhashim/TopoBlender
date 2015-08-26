#include "ManualBlend.h"
#include "ManualBlendView.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"
#include "Gallery.h"
#include "Document.h"

#include <QGraphicsProxyWidget>

#include "ui_ManualBlend.h"

ManualBlend::ManualBlend(Document *document, const QRectF &bounds) : Tool(document)
{
    setBounds(bounds);

    connect(this, SIGNAL(boundsChanged()), SLOT(resizeViews()));

    setObjectName("manualBlend");
}

void ManualBlend::init()
{
    view = new ManualBlendView(document, this);

    // Add widget
    auto toolsWidgetContainer = new QWidget();
    widget = new Ui::ManualBlendWidget();
    widget->setupUi(toolsWidgetContainer);
    widgetProxy = new QGraphicsProxyWidget(this);
    widgetProxy->setWidget(toolsWidgetContainer);

    // Fill categoies box
    {
        for(auto cat : document->categories.keys()){
            widget->categoriesBox->insertItem(widget->categoriesBox->count(), cat);
        }

        connect(widget->categoriesBox, &QComboBox::currentTextChanged, [&](QString text){
            document->currentCategory = text;
        });

        int idx = widget->categoriesBox->findText(document->categoryOf(document->firstModelName()));
        widget->categoriesBox->setCurrentIndex(idx);
    }

    // Place at bottom left corner
    auto delta = widgetProxy->sceneBoundingRect().bottomLeft() -
            scene()->views().front()->rect().bottomLeft();
    widgetProxy->moveBy(-delta.x(), -delta.y());

    // Connect UI with actions
    {
        connect(widget->analyzeButton, &QPushButton::pressed, [&](){
            document->analyze( widget->categoriesBox->currentText() );
        });
    }
}

void ManualBlend::resizeViews()
{
    view->setRect(bounds);
}
