#pragma once
#include <QGraphicsView>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    GraphicsView(QWidget *parent);
	void resizeEvent(QResizeEvent *event);
signals:
    void resized(QRectF);
};
