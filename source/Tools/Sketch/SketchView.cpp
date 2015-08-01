#include "GraphicsScene.h"

#include "Sketch.h"
#include "SketchView.h"

#include "Viewer.h"
#include "Camera.h"

#include <QGraphicsView>
#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QOpenGLFramebufferObject>

#include "Document.h"

double a = 0;

SketchView::SketchView(Document * document, QGraphicsItem *parent, SketchViewType type) :
QGraphicsObject(parent), rect(QRect(0, 0, 100, 100)), type(type), camera(nullptr), trackball(nullptr),
leftButtonDown(false), rightButtonDown(false), middleButtonDown(false), document(document)
{
	this->setFlags(QGraphicsItem::ItemIsMovable);

	camera = new Eigen::Camera();
	trackball = new Eigen::Trackball();

	if (type == VIEW_CAMERA)
	{
        // Camera target and initial position
        auto frame = camera->frame();
        frame.position = Eigen::Vector3f(-1, 0, 0.5);
        camera->setTarget(Eigen::Vector3f(0,0,0.5));
		camera->setFrame(frame);

        // Default view angle
        double theta1 = acos(-1) * 0.75;
        double theta2 = acos(-1) * 0.10;
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
        camera->zoom(-4);
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));

		trackball->setCamera(camera);
	}
    else
    {
        if(type != VIEW_TOP) camera->setTarget(Eigen::Vector3f(0,0.5f,0));
    }
}

SketchView::~SketchView()
{
	delete camera; delete trackball;
}

void SketchView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);

	// Background stuff:
	prePaint(painter, widget);

	// Begin drawing 3D
	painter->beginNativePainting();

	// Get OpenGL widget
	auto glwidget = (Viewer*)widget;
	if (glwidget)
	{
		// Viewport region
		auto rect = sceneBoundingRect();

		// OpenGL draws directly to screen, compensate for that
		QPoint viewDelta = scene()->views().first()->mapFromScene(rect.topLeft());
		if (viewDelta.manhattanLength() > 5) rect.moveTopLeft(viewDelta);

		glwidget->glViewport(rect.left(), rect.height() - rect.top(), rect.width(), rect.height());

		// Camera settings
		QMatrix4x4 cameraMatrix;
		if (type == VIEW_CAMERA)
		{
			camera->setViewport(rect.width(), rect.height());
			Eigen::Matrix4f p = camera->projectionMatrix();
			Eigen::Matrix4f v = camera->viewMatrix().matrix();
			p.transposeInPlace();
			v.transposeInPlace();
			cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());

            glwidget->eyePos = QVector3D(camera->position().x(),camera->position().y(),camera->position().z());
		}
		else
		{
			auto delta = camera->target();

			QVector3D d(delta.x(), -delta.y(), 0); // default is top view
			if (type == VIEW_FRONT) d = QVector3D(delta.x(), 0, -delta.y());
			if (type == VIEW_LEFT) d = QVector3D(0, -delta.x(), -delta.y());

			cameraMatrix = Viewer::defaultOrthoViewMatrix(type, rect.width(), rect.height(), camera->target().z());
			cameraMatrix.translate(d);

            glwidget->eyePos = QVector3D(-2,-2,2);
		}

        // Save camera at OpenGL widget
        glwidget->pvm = cameraMatrix;

        // Draw debug shape
        {
            //glwidget->drawBox(3, 1, 2, cameraMatrix);
        }

        // Draw models
        {
            document->drawModel(document->firstModelName(), glwidget);
        }

        // Grid: prepare and draw
        {
            // Build grid geometry
            QVector<QVector3D> lines;
            double gridWidth = 10;
            double gridSpacing = gridWidth / 20.0;
            for (GLfloat i = -gridWidth; i <= gridWidth; i += gridSpacing) {
                lines << QVector3D(i, gridWidth, 0); lines << QVector3D(i, -gridWidth, 0);
                lines << QVector3D(gridWidth, i, 0); lines << QVector3D(-gridWidth, i, 0);
            }

            // Grid color
            QColor color = QColor::fromRgbF(0.5, 0.5, 0.5, 0.1);

            // Rotate grid based on view type
            QMatrix4x4 rot; rot.setToIdentity();
            switch (type){
            case VIEW_TOP: rot.rotate(0, QVector3D(1, 0, 0)); break;
            case VIEW_FRONT: rot.rotate(90, QVector3D(1, 0, 0)); break;
            case VIEW_LEFT: rot.rotate(90, QVector3D(0, 1, 0)); break;
            case VIEW_CAMERA: break;
            }
            for (auto & l : lines) l = rot.map(l);

            // Draw grid
            glwidget->drawLines(lines, color, cameraMatrix);
        }
	}

	// End 3D drawing
	painter->endNativePainting();

	// Foreground stuff
	postPaint(painter, widget);
}

void SketchView::prePaint(QPainter * painter, QWidget *)
{
	// Background
	{
		auto rect = boundingRect();
		auto lightBackColor = QColor::fromRgb(124, 143, 162);
		auto darkBackColor = QColor::fromRgb(27, 30, 32);

		if (type == VIEW_CAMERA)
		{
			// Colors

			QLinearGradient gradient(0, 0, 0, rect.height());
			gradient.setColorAt(0.0, lightBackColor);
			gradient.setColorAt(1.0, darkBackColor);
			painter->fillRect(rect, gradient);
		}
		else
		{
			painter->fillRect(rect, darkBackColor);
		}
	}
}

void SketchView::postPaint(QPainter * painter, QWidget *)
{
	// View's title
	painter->setPen(QPen(Qt::white));
	painter->drawText(QPointF(10, 20), SketchViewTypeNames[type]);

	// Messages
	int x = 15, y = 45;
	for (auto message : messages){
		painter->drawText(x, y, message);
		y += 12;
	}

	// Draw link
	if (leftButtonDown || rightButtonDown || middleButtonDown)
	{
		painter->drawLine(buttonDownCursorPos, mouseMoveCursorPos);
	}

	// Border
	int bw = 3, hbw = bw * 0.5;
	QPen borderPen(this->isUnderMouse() ? Qt::yellow : Qt::gray, bw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
	painter->setPen(borderPen);
	painter->drawRect(this->boundingRect().adjusted(hbw, hbw, -hbw, -hbw));
}

void SketchView::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	mouseMoveCursorPos = event->pos();

	if ((event->buttons() & Qt::LeftButton))
	{
		if (messages.size() != 2) { messages.clear(); messages << "" << ""; }

		messages[0] = QString("start mouse pos: %1, %2").arg(buttonDownCursorPos.x()).arg(buttonDownCursorPos.y());
		messages[1] = QString("cur mouse pos: %1, %2").arg(mouseMoveCursorPos.x()).arg(mouseMoveCursorPos.y());

		// DEBUG camera:
		messages << QString("%1,%2,%3,%4,")
			.arg(camera->frame().orientation.x())
			.arg(camera->frame().orientation.y())
			.arg(camera->frame().orientation.z())
			.arg(camera->frame().orientation.w());

		messages << QString("%1,%2,%3")
			.arg(camera->frame().position.x())
			.arg(camera->frame().position.y())
			.arg(camera->frame().position.z());
	}

	// Camera movement
    if(rightButtonDown)
    {
        if (type == SketchViewType::VIEW_CAMERA)
        {
            trackball->track(Eigen::Vector2i(mouseMoveCursorPos.x(), mouseMoveCursorPos.y()));
        }
        else
        {
            double scale = 0.01;
            auto delta = scale * (buttonDownCursorPos - mouseMoveCursorPos);
            camera->setTarget(camera->start + Eigen::Vector3f(delta.x(), delta.y(), 0));
        }
    }

    scene()->update();
}

void SketchView::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	// Record mouse states
	buttonDownCursorPos = event->pos();
	mouseMoveCursorPos = buttonDownCursorPos;
	if (event->buttons() & Qt::LeftButton) leftButtonDown = true;
	if (event->buttons() & Qt::RightButton) rightButtonDown = true;
	if (event->buttons() & Qt::MiddleButton) middleButtonDown = true;

	// Camera movement
    if(rightButtonDown)
    {
        if (type == SketchViewType::VIEW_CAMERA)
        {
            trackball->start(Eigen::Trackball::Around);
            trackball->track(Eigen::Vector2i(buttonDownCursorPos.x(), buttonDownCursorPos.y()));
        }
        else
        {
            camera->start = camera->target();
        }
    }

    scene()->update();
}

void SketchView::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
	// Record mouse states
	buttonUpCursorPos = event->pos();
	if (event->button() == Qt::LeftButton) leftButtonDown = false;
	if (event->button() == Qt::RightButton) rightButtonDown = false;
	if (event->button() == Qt::MiddleButton) middleButtonDown = false;

	messages.clear(); messages << "Mouse released." <<
		QString("%1,%2").arg(buttonUpCursorPos.x()).arg(buttonUpCursorPos.y());

    scene()->update();
}

void SketchView::wheelEvent(QGraphicsSceneWheelEvent * event)
{
	QGraphicsObject::wheelEvent(event);

	double step = (double)event->delta() / 120;

	// View zoom
	if (type == SketchViewType::VIEW_CAMERA)
	{
		camera->zoom(step);
	}
	else
	{
		auto t = camera->target();
		double speed = 0.25;
		t.z() += (step * speed);
		camera->setTarget(t);
	}

    scene()->update();
}
