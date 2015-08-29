#include "Gallery.h"
#include "Thumbnail.h"

#include <QPainter>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsDropShadowEffect>

Gallery::Gallery(QGraphicsItem *parent, QRectF rect, int numColumns, QRectF defaultItemRect, bool isFixedSize) : QGraphicsObject(parent),
rect(rect), numColumns(numColumns), defaultItemRect(defaultItemRect)
{
    setFlag( QGraphicsItem::ItemClipsChildrenToShape );

    // Create scrollbar
    if(!isFixedSize)
	{
		int sw = 16;
        scrollbar = new QScrollBar(Qt::Vertical);
        scrollbar->setMinimum(0);
        scrollbar->setMaximum(99);
        scrollbar->setValue(0);
        QRect scrollbarRect = QRect(rect.width() - sw, 0, sw, rect.height());
        if(scrollbar->orientation() == Qt::Horizontal)
            scrollbarRect = QRect(0, rect.height() - sw, rect.width(), sw);
        scrollbar->setGeometry(scrollbarRect);

        auto scrollbarProxy = new QGraphicsProxyWidget(this);
        scrollbarProxy->setWidget(scrollbar);
        scrollbarProxy->setZValue(999);
		scrollbarProxy->setOpacity(0.25);

        connect(scrollbar, &QScrollBar::valueChanged, [&](int value){
            double t = double(value) / scrollbar->maximum();
            double galleryHeight = nextItemRect().top() - rect.height();
            double delta = t * -galleryHeight;
            for(auto item : items){
                QRectF r = item->data["rectInGallery"].value<QRectF>();
                item->setPos(r.topLeft() + QPointF(0,delta));
            }
        });
    }
}

void Gallery::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->fillRect(rect, QColor(0, 0, 0, 100));
}

Thumbnail * Gallery::addImageItem(QImage image, QVariantMap data)
{
    auto t = new Thumbnail(this, defaultItemRect);
    t->setImage(image);
    t->setData(data);
    items.push_back(t);

    auto r = nextItemRect();
    t->moveBy(r.topLeft().x(), r.topLeft().y());
    t->data["rectInGallery"].setValue(r);

	return t;
}

Thumbnail * Gallery::addTextItem(QString text, QVariantMap data)
{
    auto t = new Thumbnail(this, defaultItemRect);
    t->setCaption(text);
    t->setData(data);
    t->setMesh(Thumbnail::QBasicMesh());
    items.push_back(t);

    auto r = nextItemRect();
    t->moveBy(r.topLeft().x(), r.topLeft().y());
    t->data["rectInGallery"].setValue(r);

	return t;
}

Thumbnail * Gallery::addMeshItem(Thumbnail::QBasicMesh mesh, QVariantMap data)
{
    auto t = new Thumbnail(this, defaultItemRect);
	t->set(QImage(), "", mesh);
	t->setData(data);
    items.push_back(t);

    auto r = nextItemRect();
    t->moveBy(r.topLeft().x(), r.topLeft().y());
    t->data["rectInGallery"].setValue(r);

	return t;
}

QRectF Gallery::nextItemRect()
{
    int fullLength = (items.size()-1) * defaultItemRect.width();
    int row = fullLength / rect.width();
    int rowWidth = fullLength - (row * rect.width());
    int col = rowWidth / (int)defaultItemRect.width();
    double x = col * defaultItemRect.width();
    double y = row * defaultItemRect.height();
    return QRectF(x,y,defaultItemRect.width(), defaultItemRect.height());
}

void Gallery::clearThumbnails()
{
    for(auto t : items) t->deleteLater();
}

QVector<Thumbnail *> Gallery::getSelected()
{
    QVector<Thumbnail *> result;
    for(auto t : items) if(t->isSelected()) result << t;
    return result;
}

void Gallery::wheelEvent(QGraphicsSceneWheelEvent * event)
{
    double step = (double)event->delta() / 120;
    double delta = step * 5;
    scrollbar->setValue(scrollbar->value() - delta);
}
