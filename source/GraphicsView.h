#pragma once
#include <QGraphicsView>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    GraphicsView(QWidget *parent);
protected:
	void resizeEvent(QResizeEvent *event);
    void keyPressEvent(QKeyEvent* event);
signals:
    void resized(QRectF);
    void globalSettingsChanged();
};
