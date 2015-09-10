#pragma once
#include <QGraphicsObject>
#include "Thumbnail.h"

#include <QScrollBar>

class Gallery : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    Gallery(QGraphicsItem *parent, QRectF rect = QRectF(0, 0, 512, 300), QRectF defaultItemRect = QRectF(0, 0, 128, 128), bool isFixedSize = false);

	QRectF rect;
    QRectF boundingRect() const { return rect; }
    void setRect(const QRectF & newRect);

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);

    QRectF defaultItemRect;

	Thumbnail * addImageItem(QImage image, QVariantMap data = QVariantMap());
	Thumbnail * addTextItem(QString text, QVariantMap data = QVariantMap());
	Thumbnail * addMeshItem(Thumbnail::QBasicMesh mesh, QVariantMap data = QVariantMap());

    QRectF nextItemRect();

    void clearThumbnails();
    QVector<Thumbnail *> getSelected();

//protected:
    QVector<Thumbnail*> items;
    QScrollBar * scrollbar;

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *);
};
