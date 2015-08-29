#pragma once
#include "Tool.h"

class ManualBlendView;
class ManualBlendManager;
namespace Ui{ class ManualBlendWidget; }

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
