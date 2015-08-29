#pragma once
#include "Tool.h"
#include <QMatrix4x4>

namespace Ui{ class ExploreWidget; }
class Thumbnail;

class Explore : public Tool
{
    Q_OBJECT
public:
    Explore(Document * document, const QRectF &bounds);
    void init();
protected:
    Ui::ExploreWidget* widget;
    QGraphicsProxyWidget* widgetProxy;

    QMatrix4x4 cameraMatrix;
    QVector3D cameraPos;
};
