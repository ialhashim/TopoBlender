#pragma once
#include "Tool.h"

class ManualBlendView;
namespace Ui{ class ManualBlendWidget; }
class QGraphicsProxyWidget;

class ManualBlend : public Tool
{
    Q_OBJECT
public:
    ManualBlend(Document * document, const QRectF &bounds);

    void init();

protected:
    ManualBlendView * view;
    Ui::ManualBlendWidget * widget;
    QGraphicsProxyWidget *widgetProxy;

public slots:
    void resizeViews();
};
