#pragma once
#include "Tool.h"
#include <QMatrix4x4>
#include <QMap>

namespace Ui{ class ExploreWidget; }
class Thumbnail;
class ExploreLiveView;

class Explore : public Tool
{
    Q_OBJECT
public:
    Explore(Document * document, const QRectF &bounds);
    void init();
protected:
    Ui::ExploreWidget* widget;
    QGraphicsProxyWidget* widgetProxy;

    QMap<QString, Thumbnail*> thumbs;
    QMap<QPair<int,int>, QGraphicsLineItem*> lines;

    ExploreLiveView * liveView;

    QMatrix4x4 cameraMatrix;
    QVector3D cameraPos;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);

    QString startShape, targetShape;
    double alpha;

    QMap<int, QMap<int,double> > distMat;
    double max_val, min_val;
};
