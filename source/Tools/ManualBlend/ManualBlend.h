#pragma once
#include "Tool.h"

class ManualBlendView;
namespace Ui{ class ManualBlendWidget; }
class QGraphicsProxyWidget;
class ManualBlendManager;

class ManualBlend : public Tool
{
    Q_OBJECT
public:
    ManualBlend(Document * document, const QRectF &bounds);

    void init();

protected:
    ManualBlendView* view;
    Ui::ManualBlendWidget* widget;
    QGraphicsProxyWidget* widgetProxy;
	ManualBlendManager* blendManager;

public slots:
    void resizeViews();
};
