#pragma once
#include "Tool.h"
#include "SketchView.h"

class Sketch : public Tool
{
    Q_OBJECT
public:
    Sketch(const QRectF& bounds);

    void init();

    QVector<SketchView*> views;

public slots:
    void resizeViews();
};

