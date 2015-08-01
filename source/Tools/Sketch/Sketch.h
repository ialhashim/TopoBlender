#pragma once
#include "Tool.h"
#include "SketchView.h"

class Sketch : public Tool
{
    Q_OBJECT
public:
    Sketch(Document * document, const QRectF& bounds);

    void init();

    QVector<SketchView*> views;

    Document * document;

public slots:
    void resizeViews();
};

