#pragma once

#include <QObject>

#include "ShapeGraph.h"
#include "Viewer.h"

class Model : public QObject, public Structure::ShapeGraph
{
    Q_OBJECT
public:
    explicit Model(QObject *parent = 0);

    void draw(Viewer * glwidget);

signals:

public slots:
};
