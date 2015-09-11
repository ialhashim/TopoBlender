#include "ExploreLiveView.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>

#include "Document.h"

#include "Viewer.h"

#include "ExploreProcess.h"

ExploreLiveView::ExploreLiveView(QGraphicsItem *parent, Document *document) : QGraphicsObject(parent), document(document)
{
    this->setFlag(QGraphicsItem::ItemIsSelectable);
    this->setFlag(QGraphicsItem::ItemIsMovable);
}

void ExploreLiveView::showBlend(QVariantMap info)
{
    QString source = info["source"].toString();
    QString target = info["target"].toString();
    double alpha = info["alpha"].toDouble();

    auto key = qMakePair(source,target);

    if(!blendPath.contains(key))
        blendPath[key] = QSharedPointer<ExploreProcess::BlendPath>(new ExploreProcess::BlendPath);

    auto & path = blendPath[qMakePair(source,target)];

    path->source = source;
    path->target = target;
    path->alpha = alpha;
    path->prepare(document);

    this->shapeRect = QRectF(0,0,128,128);

    meshes = path->blend();
}

void ExploreLiveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget * widget)
{
    prePaint(painter);

    auto glwidget = (Viewer*)widget;
    if (glwidget && meshes.size())
    {
        QRectF parentRect = parentItem()->sceneBoundingRect();

        painter->beginNativePainting();

        // Draw mesh
        auto r = sceneBoundingRect();
        auto v = scene()->views().first();
        QPoint viewDelta = v->mapFromScene(r.topLeft());
        if (viewDelta.manhattanLength() > 5) r.moveTopLeft(viewDelta);

        auto camera = ExploreProcess::defaultCamera(document->extent().length());

        glwidget->eyePos = camera.first;
        glwidget->pvm = camera.second;
        glwidget->glViewport(r.left(), v->height() - r.height() - r.top(), r.width(), r.height());

        // Clipping OpenGL
        glwidget->glEnable(GL_SCISSOR_TEST);
        glwidget->glScissor(parentRect.x(), v->height() - parentRect.height() - parentRect.top(), parentRect.width(), parentRect.height());

        glwidget->glClear(GL_DEPTH_BUFFER_BIT);

        // Draw aux meshes
        for (auto mesh : meshes)
        {
            if(mesh.isPoints)
                glwidget->drawOrientedPoints( mesh.points, mesh.normals, mesh.color, glwidget->pvm);
            else
                glwidget->drawTriangles( mesh.color, mesh.points, mesh.normals, glwidget->pvm );
        }

        glwidget->glDisable(GL_SCISSOR_TEST);

        painter->endNativePainting();
    }

    postPaint(painter);
}

void ExploreLiveView::prePaint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::blue);
}

void ExploreLiveView::postPaint(QPainter *painter)
{
    painter->setPen(QPen(Qt::green, 2));
    painter->drawRect(boundingRect());
}
