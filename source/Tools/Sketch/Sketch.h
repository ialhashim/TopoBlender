#pragma once
#include "Tool.h"

namespace Ui{ class SketchToolsWidget; }
class QGraphicsProxyWidget;
class SketchDuplicate;
class SketchView;

class Sketch : public Tool
{
    Q_OBJECT
public:
    Sketch(Document * document, const QRectF& bounds);

    void init();

    QVector<SketchView*> views;

    Ui::SketchToolsWidget * toolsWidget;
    SketchDuplicate * dupToolWidget;
    QGraphicsProxyWidget * dupToolWidgetProxy;

public slots:
    void resizeViews();
};

