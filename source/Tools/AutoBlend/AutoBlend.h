#pragma once
#include "Tool.h"
#include <QMatrix4x4>

namespace Ui{ class AutoBlendWidget; }
class Gallery;

class AutoBlend : public Tool
{
    Q_OBJECT
public:
    AutoBlend(Document * document, const QRectF &bounds);
    void init();

protected:
    Ui::AutoBlendWidget* widget;
    QGraphicsProxyWidget* widgetProxy;

    Gallery * gallery;
    Gallery * results;

	QMatrix4x4 cameraMatrix;
	QVector3D cameraPos;
public slots:
	void doBlend();
};
