#include "GraphicsScene.h"
#include "GeometryHelper.h"

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
#include "Model.h"

static Eigen::Vector3f toVector3(QVector3D p){ return Eigen::Vector3f(p[0], p[1], p[2]); }

SketchView::SketchView(Document * document, QGraphicsItem *parent, SketchViewType type) :
QGraphicsObject(parent), rect(QRect(0, 0, 100, 100)), type(type), camera(nullptr), trackball(nullptr),
leftButtonDown(false), rightButtonDown(false), middleButtonDown(false), document(document), sketchOp(SKETCH_NONE)
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

    // Sketching plane
    switch(type){
    case VIEW_TOP:
        sketchPlane = new Eigen::Plane(Eigen::Vector3f::UnitZ(), Eigen::Vector3f(0,0,0.5f));
        break;
    case VIEW_FRONT:
        sketchPlane = new Eigen::Plane(-Eigen::Vector3f::UnitY(), Eigen::Vector3f(0,0,0.5f));
        break;
    case VIEW_LEFT:
        sketchPlane = new Eigen::Plane(-Eigen::Vector3f::UnitX(), Eigen::Vector3f(0,0,0.5f));
        break;
    default: { sketchPlane = new Eigen::Plane(Eigen::Vector3f::UnitZ(), Eigen::Vector3f(0,0,0.5f)); break;}
    }

	// Manipulator tool
	manTool = new SketchManipulatorTool(this);
	manTool->setVisible(false);
	manTool->view = this;
	manTool->model = document->getModel(document->firstModelName());

	manTool->model->connect(manTool, SIGNAL(transformChange(QMatrix4x4)), SLOT(transformActiveNodeGeometry(QMatrix4x4)));
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

            eyePos = QVector3D(camera->position().x(),camera->position().y(),camera->position().z());
		}
		else
		{
            QVector3D eye(-2,-2,2);

			auto delta = camera->target();

			QVector3D d(delta.x(), -delta.y(), 0); // default is top view
			if (type == VIEW_FRONT) d = QVector3D(delta.x(), 0, -delta.y());
			if (type == VIEW_LEFT) d = QVector3D(0, -delta.x(), -delta.y());

            cameraMatrix = SketchView::defaultOrthoViewMatrix(eye, type, rect.width(), rect.height(),
                                                              d, camera->target().z(), projectionMatrix, viewMatrix);

            // Should only be used for lighting
            eyePos = eye + d;
		}

        // Update local camera details
        glwidget->eyePos = eyePos;

        // Temporarly save active camera at OpenGL widget
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
            glwidget->glLineWidth(1.0f);
            glwidget->drawLines(lines, color, cameraMatrix, "grid_lines");
        }

        // Visualize sketching
        glwidget->drawPoints(sketchPoints, sketchOp == DEFORM_SKETCH ? Qt::green : Qt::red, cameraMatrix, true);

        bool isVisualizeSketchPlane = false;
        if(sketchPlane != nullptr && isVisualizeSketchPlane)
        {
            auto n = toQVector3D(sketchPlane->normal());
            auto o = toQVector3D(Eigen::Vector3f::Zero() - (sketchPlane->normal() * sketchPlane->offset()));
            glwidget->drawPlane(n,o, cameraMatrix);
        }
	}

	// End 3D drawing
	painter->endNativePainting();

    // Simple rectangle for sketching sheets
    if(leftButtonDown && sketchOp == SKETCH_SHEET)
    {
        QRectF r(buttonDownCursorPos, mouseMoveCursorPos);

        painter->fillRect(r, QColor(0,0,255,128));
        painter->setPen(QPen(Qt::blue,3));
        painter->drawRect(r);
    }

	// Foreground stuff
	postPaint(painter, widget);
}

void SketchView::prePaint(QPainter * painter, QWidget *)
{
	// Background
	{
		auto rect = boundingRect();

        // Colors
		auto lightBackColor = QColor::fromRgb(124, 143, 162);
		auto darkBackColor = QColor::fromRgb(27, 30, 32);

		if (type == VIEW_CAMERA)
        {
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
    {
        painter->setPen(QPen(Qt::white));
        painter->drawText(QPointF(10, 20), SketchViewTypeNames[type]);
    }

    // Draw current operation title
    {
        painter->drawText(QPointF(10, rect.height()-20), SketchViewOpName[sketchOp]);
    }

	// Messages
    {
        int x = 15, y = 45;
        for (auto message : messages){
            painter->drawText(x, y, message);
            y += 12;
        }
    }

	// Draw link
	if (leftButtonDown || rightButtonDown || middleButtonDown)
	{
		painter->drawLine(buttonDownCursorPos, mouseMoveCursorPos);
	}

	// Border
    {
        int bw = 3, hbw = bw * 0.5;
        QPen borderPen(this->isUnderMouse() ? Qt::yellow : Qt::gray, bw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        painter->setPen(borderPen);
        painter->drawRect(this->boundingRect().adjusted(hbw, hbw, -hbw, -hbw));
    }

	// Update manipulator tool
	/*if (manTool->isVisible() && !this->isUnderMouse())
	{
		auto model = document->getModel(document->firstModelName());
		if (model != nullptr) {
			auto n = model->lastAddedNode;
			if (n != nullptr){
				auto screenPos = worldToScreen(toQVector3D(n->center()));
				manTool->setPos(screenPos.toPointF() - manTool->boundingRect().center());
			}
		}
	}*/
}

QMatrix4x4 SketchView::defaultOrthoViewMatrix(QVector3D & eye, int type, int w, int h,
                                              QVector3D translation, float zoomFactor, QMatrix4x4 &proj, QMatrix4x4 &view)
{
    QMatrix4x4 pvMatrix;
    proj.setToIdentity();
    view.setToIdentity();

    float ratio = float(w) / h;
    float orthoZoom = 1 + zoomFactor;
    float orthoEye = 10;

    // Default is top view
    QVector3D focalPoint(0, 0, 0);
    QVector3D upVector(0.0, 1.0, 0.0);
    eye = QVector3D(0, 0, orthoEye);

    if (type == 1){ /* FRONT */
        eye = QVector3D(0, -orthoEye, 0);
        upVector = QVector3D(0,0,1);
    }

    if (type == 2){ /* LEFT */
        eye = QVector3D(-orthoEye, 0, 0);
        upVector = QVector3D(0,0,1);
    }

    proj.ortho(orthoZoom * -ratio, orthoZoom * ratio, orthoZoom * -1, orthoZoom * 1, 0.01f, 100.0f);
    view.lookAt(eye, focalPoint, upVector);
    view.translate(translation);

    pvMatrix = proj * view;
    return pvMatrix;
}

QVector3D SketchView::screenToWorld(QPointF point2D)
{
    return screenToWorld(QVector3D(point2D.x(), point2D.y(), 0));
}

QVector3D SketchView::screenToWorld(QVector3D point2D)
{
    float x = (2.0 * point2D.x() * (1.0/rect.width())) - 1.0;
    float y = -(2.0 * point2D.y() * (1.0/rect.height())) + 1.0;
    float z = point2D.z();

    return (projectionMatrix * viewMatrix).inverted().map(QVector3D(x,y,z));
}

QVector3D SketchView::worldToScreen(QVector3D point3D)
{
    QVector3D p = (projectionMatrix * viewMatrix).map(point3D);
    return QVector3D((1.0+p.x()) * rect.width() * 0.5,
                     (1.0-p.y()) * rect.height() * 0.5,
                     (0.0+p.z()));
}

void SketchView::setSketchOp(SketchViewOp toSketchOp)
{
	this->sketchOp = toSketchOp;

	// Manipulator tool
	if (this->sketchOp == TRANSFORM_PART && type != VIEW_CAMERA)
	{
		auto model = document->getModel(document->firstModelName());
		if (model != nullptr) {
			auto n = model->activeNode;
			if (n != nullptr) {
				// Place manipulator at center of part
				auto screenPos = worldToScreen(toQVector3D(n->center()));
				manTool->setPos(screenPos.toPointF() - manTool->boundingRect().center());
				manTool->setVisible(true);
			}
		}
	}
	else
	{
		manTool->setVisible(false);
	}
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

    // Free form Sketching
    if(leftButtonDown)
	{
		auto worldPos = screenToWorld(event->pos());

        if(sketchOp == SKETCH_CURVE)
        {
            sketchPoints << toQVector3D( sketchPlane->projection(toVector3(worldPos)) );
        }

        if(sketchOp == SKETCH_SHEET)
        {

        }

        if(sketchOp == DEFORM_SKETCH)
        {
            sketchPoints << toQVector3D( sketchPlane->projection(toVector3(worldPos)) );
        }

		if(sketchOp == TRANSFORM_PART)
		{

		}
    }

	// Constrained sketching
	if (leftButtonDown && event->modifiers().testFlag(Qt::ShiftModifier))
	{
		auto startWorldPos = screenToWorld(buttonDownCursorPos);
		auto worldPos = screenToWorld(event->pos());

		if (sketchOp == SKETCH_CURVE)
		{
			sketchPoints.clear();
			sketchPoints << toQVector3D(sketchPlane->projection(toVector3(startWorldPos)));
			sketchPoints << toQVector3D(sketchPlane->projection(toVector3(worldPos)));
		}

		if (sketchOp == DEFORM_SKETCH)
		{
			sketchPoints.clear();
			sketchPoints << toQVector3D(sketchPlane->projection(toVector3(startWorldPos)));
			sketchPoints << toQVector3D(sketchPlane->projection(toVector3(worldPos)));
		}
	}

    scene()->update(sceneBoundingRect());
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

    if(leftButtonDown)
    {
        if(sketchOp == SKETCH_CURVE)
        {
            sketchPoints.clear();
        }

        if(sketchOp == SKETCH_SHEET)
        {
            sketchPoints.clear();
        }

        if(sketchOp == DEFORM_SKETCH)
        {
            sketchPoints.clear();
        }

		if(sketchOp == TRANSFORM_PART)
		{

		}
    }

	scene()->update(sceneBoundingRect());
}

void SketchView::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
	// Record mouse states
	buttonUpCursorPos = event->pos();

    if(event->button() == Qt::LeftButton)
    {
        if(sketchOp == SKETCH_CURVE)
        {
            document->createCurveFromPoints(document->firstModelName(), sketchPoints);
        }

        if(sketchOp == SKETCH_SHEET)
        {
            QRectF r(buttonDownCursorPos, mouseMoveCursorPos);
            sketchPoints << toQVector3D( sketchPlane->projection(toVector3(screenToWorld(r.topLeft()))));
            sketchPoints << toQVector3D( sketchPlane->projection(toVector3(screenToWorld(r.topRight()))));
            sketchPoints << toQVector3D( sketchPlane->projection(toVector3(screenToWorld(r.bottomLeft()))));
            document->createSheetFromPoints(document->firstModelName(), sketchPoints);
        }

        if(sketchOp == DEFORM_SKETCH)
        {
            auto guidePoints = sketchPoints;
            guidePoints.push_front(toQVector3D(sketchPlane->normal()));

            document->modifyLastAdded(document->firstModelName(), guidePoints);
        }
    }

    if (event->button() == Qt::LeftButton) leftButtonDown = false;
    if (event->button() == Qt::RightButton) rightButtonDown = false;
    if (event->button() == Qt::MiddleButton) middleButtonDown = false;

	messages.clear(); messages << "Mouse released." <<
		QString("%1,%2").arg(buttonUpCursorPos.x()).arg(buttonUpCursorPos.y());

	sketchPoints.clear();

	scene()->update(sceneBoundingRect());
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

	scene()->update(sceneBoundingRect());
}
