#pragma once
#include "Tool.h"
#include "SketchView.h"

namespace Ui{ class SketchToolsWidget; }

class Sketch : public Tool
{
    Q_OBJECT
public:
    Sketch(Document * document, const QRectF& bounds);

    void init();

    QVector<SketchView*> views;

    Document * document;

    Ui::SketchToolsWidget * toolsWidget;
public slots:
    void resizeViews();
};

