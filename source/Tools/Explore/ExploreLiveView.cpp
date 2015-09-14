#include "ExploreLiveView.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>

#include "Document.h"

#include "Viewer.h"
#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>

#include "ExploreProcess.h"

ExploreLiveView::ExploreLiveView(QGraphicsItem *parent, Document *document) : QGraphicsObject(parent),
    document(document), isReady(false), isCacheImage(false), cacheImageSize(512)
{
	this->setFlag(QGraphicsItem::ItemIsSelectable);
	this->setFlag(QGraphicsItem::ItemIsMovable);
}

void ExploreLiveView::showBlend(QVariantMap info)
{
    this->isReady = false;

	QString source = info["source"].toString();
	QString target = info["target"].toString();
	double alpha = info["alpha"].toDouble();

	auto key = qMakePair(source, target);

	if (!blendPath.contains(key))
		blendPath[key] = QSharedPointer<ExploreProcess::BlendPath>(new ExploreProcess::BlendPath);

	auto & path = blendPath[qMakePair(source, target)];

	path->source = source;
	path->target = target;
	path->alpha = std::min(1.0, std::max(0.0, alpha));
	path->prepare(document);

	this->shapeRect = QRectF(0, 0, 128, 128);

    meshes = path->blend();

    if(info["hqRendering"].toBool())
    {
        this->isCacheImage = true;
        this->cachedImage = QImage();
    }

    this->isReady = true;
}

void ExploreLiveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget * widget)
{
    if(!isReady) return;

	prePaint(painter);

	postPaint(painter);

	auto glwidget = (Viewer*)widget;
    if (glwidget && meshes.size())
	{
		QRectF parentRect = parentItem()->sceneBoundingRect();

        if (isCacheImage && cachedImage.isNull())
        {
            QOpenGLContext context;
            context.setShareContext(glwidget->context());
            context.setFormat(glwidget->format());
            context.create();

            QOffscreenSurface m_offscreenSurface;
            m_offscreenSurface.setFormat(context.format());
            m_offscreenSurface.create();

            context.makeCurrent(&m_offscreenSurface);

            QOpenGLFramebufferObjectFormat fboformat;
            fboformat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            QOpenGLFramebufferObject renderFbo(cacheImageSize*1.5, cacheImageSize, fboformat);
            renderFbo.bind();

            glwidget->glEnable(GL_DEPTH_TEST);
            glwidget->glEnable(GL_BLEND);
            glwidget->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glwidget->glCullFace(GL_BACK);

            glwidget->glClearColor(0,0,0,0);
            glwidget->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glwidget->glViewport(0, 0, cacheImageSize*1.5, cacheImageSize);

            glwidget->glPointSize(10);

            // Draw aux meshes
            for (auto mesh : meshes)
            {
                if (mesh.isPoints)
                    glwidget->drawOrientedPoints(mesh.points, mesh.normals, mesh.color, glwidget->pvm);
                else
                    glwidget->drawTriangles(mesh.color, mesh.points, mesh.normals, glwidget->pvm);
            }

            glwidget->glDisable(GL_DEPTH_TEST);
            glwidget->glFlush();

            renderFbo.release();

            cachedImage = renderFbo.toImage();
            isReady = true;

            // Thanks for sharing!
            glwidget->makeCurrent();
        }

        // Draw as image
        if(isCacheImage)
        {
            int w = shapeRect.width();
            painter->drawImage(w * -0.5, w * -0.5, cachedImage.scaledToWidth(w));
        }

        if(!isCacheImage)
        {
            auto r = shapeRect;

            // scale view
            double s = 1.5;
            r.setWidth(r.width() * s);
            r.setHeight(r.height() * s);

            r.moveCenter(this->mapToScene(boundingRect().center()));

            painter->beginNativePainting();

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

            glwidget->glPointSize(2);

            // Draw aux meshes
            for (auto mesh : meshes)
            {
                if (mesh.isPoints)
                    glwidget->drawOrientedPoints(mesh.points, mesh.normals, mesh.color, glwidget->pvm);
                else
                    glwidget->drawTriangles(mesh.color, mesh.points, mesh.normals, glwidget->pvm);
            }

            glwidget->glDisable(GL_SCISSOR_TEST);

            painter->endNativePainting();
        }
	}
}

void ExploreLiveView::prePaint(QPainter *painter)
{
	if (meshes.empty()) return;

	QRadialGradient grad(QPointF(0, 0), shapeRect.width() * 0.5);
	grad.setColorAt(0, QColor(0, 0, 0, 200));
	grad.setColorAt(0.75, QColor(0, 0, 0, 200));
	grad.setColorAt(1, Qt::transparent);
	auto rect = shapeRect;
	rect.moveCenter(QPointF(0, 0));
	painter->fillRect(rect, grad);
}

void ExploreLiveView::postPaint(QPainter *painter)
{
	QColor penColor = Qt::white;
	penColor.setAlphaF(0.25);

	painter->setPen(QPen(penColor, 3));
	QRectF circle = shapeRect;
	circle.moveCenter(QPointF(0, 0));
	painter->drawEllipse(circle);
}
