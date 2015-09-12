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
}

void ExploreLiveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget * widget)
{
	prePaint(painter);

	postPaint(painter);

	auto glwidget = (Viewer*)widget;
	if (glwidget && meshes.size())
	{
		QRectF parentRect = parentItem()->sceneBoundingRect();

		painter->beginNativePainting();

		// Draw mesh
		auto r = shapeRect;
		r.moveCenter(this->mapToScene(boundingRect().center()));

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
			if (mesh.isPoints)
				glwidget->drawOrientedPoints(mesh.points, mesh.normals, mesh.color, glwidget->pvm);
			else
				glwidget->drawTriangles(mesh.color, mesh.points, mesh.normals, glwidget->pvm);
		}

		glwidget->glDisable(GL_SCISSOR_TEST);

		painter->endNativePainting();
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
