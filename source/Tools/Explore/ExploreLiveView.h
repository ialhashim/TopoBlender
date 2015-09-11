#pragma once
#include <QGraphicsObject>
#include <QVector3D>

class Document;
namespace ExploreProcess{  struct BlendPath; }

#include "Thumbnail.h"

class ExploreLiveView : public QGraphicsObject
{
    Q_OBJECT
public:
    ExploreLiveView(QGraphicsItem *parent, Document * document);

    QRectF boundingRect() const { return childrenBoundingRect(); }

    void showBlend(QVariantMap info);

protected:
    Document * document;

    QVector<Thumbnail::QBasicMesh> meshes;
    QString message;

    QMap< QPair<QString,QString>, QSharedPointer<ExploreProcess::BlendPath> > blendPath;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);
    void prePaint(QPainter *painter);
    void postPaint(QPainter *painter);
};
