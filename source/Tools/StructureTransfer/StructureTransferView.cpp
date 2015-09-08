#include "StructureTransferView.h"

#include "GraphicsView.h"
#include "GraphicsScene.h"
#include "Viewer.h"
#include "Camera.h"

#include "Document.h"
#include "Model.h"

#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>

StructureTransferView::StructureTransferView(Document *document, QGraphicsItem * parent) : QGraphicsObject(parent), document(document)
{
    this->setFlags(QGraphicsItem::ItemIsFocusable);
	 
    // Create camera & trackball
    {
        camera = new Eigen::Camera();
        trackball = new Eigen::Trackball();

        // Camera target and initial position
        auto frame = camera->frame();
        frame.position = Eigen::Vector3f(-1, 0, 0.5);
        camera->setTarget(Eigen::Vector3f(0,0,0.5));
        camera->setFrame(frame);

        int deltaZoom = document->extent().length() * 0.25;

        // Default view angle
        double theta1 = acos(-1) * 0.75;
        double theta2 = acos(-1) * 0.10;
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
        camera->zoom(-(4+deltaZoom));
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));

        trackball->setCamera(camera);
    }
}

StructureTransferView::~StructureTransferView()
{
    delete camera; delete trackball;
}

void StructureTransferView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget * widget)
{
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
        glwidget->glViewport(rect.left(), rect.top(), rect.width(), rect.height());

        // Camera settings
        QMatrix4x4 cameraMatrix;
        camera->setViewport(rect.width(), rect.height());
        Eigen::Matrix4f p = camera->projectionMatrix();
        Eigen::Matrix4f v = camera->viewMatrix().matrix();
        p.transposeInPlace();
        v.transposeInPlace();
        cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());

        // Update local camera details
        glwidget->eyePos = QVector3D(camera->position().x(),camera->position().y(),camera->position().z());

        // Temporarily save active camera at OpenGL widget
        glwidget->pvm = cameraMatrix;

        // Draw debug shape
        //glwidget->drawBox(3, 1, 2, cameraMatrix);

        // Draw models
        {
            document->drawModel(document->firstModelName(), glwidget);
        }

        // Grid: prepare and draw
        {
            // Build grid geometry
            QVector<QVector3D> lines;
            double gridWidth = 10;
            double gridSpacing = gridWidth / 30.0;
            for (GLfloat i = -gridWidth; i <= gridWidth; i += gridSpacing) {
                lines << QVector3D(i, gridWidth, 0); lines << QVector3D(i, -gridWidth, 0);
                lines << QVector3D(gridWidth, i, 0); lines << QVector3D(-gridWidth, i, 0);
            }

            // Grid color
            QColor color = QColor::fromRgbF(0.5, 0.5, 0.5, 0.1);

            // Draw grid
            glwidget->glLineWidth(1.0f);
            glwidget->drawLines(lines, color, cameraMatrix, "grid_lines");
        }
    }

    // End 3D drawing
    painter->endNativePainting();

    postPaint(painter, widget);
}

void StructureTransferView::prePaint(QPainter *painter, QWidget *)
{
    // Background
    auto lightBackColor = QColor::fromRgb(124, 143, 162);
    auto darkBackColor = QColor::fromRgb(27, 30, 32);

    QLinearGradient gradient(0, 0, 0, rect.height());
    gradient.setColorAt(0.0, lightBackColor);
    gradient.setColorAt(1.0, darkBackColor);
    painter->fillRect(rect, gradient);
}

void StructureTransferView::postPaint(QPainter *, QWidget *)
{
    // Border
    //painter->setPen(QPen(Qt::gray, 10));
    //painter->drawRect(rect);
}

void StructureTransferView::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
    mouseMoveCursorPos = event->pos();

    if(rightButtonDown)
    {
        trackball->track(Eigen::Vector2i(mouseMoveCursorPos.x(), mouseMoveCursorPos.y()));
    }

    scene()->update(sceneBoundingRect());
}

void StructureTransferView::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    // Record mouse states
    buttonDownCursorPos = event->pos();
    mouseMoveCursorPos = buttonDownCursorPos;
	leftButtonDown = event->buttons() & Qt::LeftButton;
	rightButtonDown = event->buttons() & Qt::RightButton;
	middleButtonDown = event->buttons() & Qt::MiddleButton;

    // Camera movement
    if(rightButtonDown)
    {
        trackball->start(Eigen::Trackball::Around);
        trackball->track(Eigen::Vector2i(buttonDownCursorPos.x(), buttonDownCursorPos.y()));
    }

	if (leftButtonDown)
	{

	}

    scene()->update(sceneBoundingRect());
}

void StructureTransferView::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    this->setFocus();

	leftButtonDown = false;
	rightButtonDown = false;
	middleButtonDown = false;

    scene()->update(sceneBoundingRect());
}

void StructureTransferView::wheelEvent(QGraphicsSceneWheelEvent * event)
{
    QGraphicsObject::wheelEvent(event);
    double step = (double)event->delta() / 120;
    camera->zoom(step);

    scene()->update(sceneBoundingRect());
}

void StructureTransferView::keyPressEvent(QKeyEvent *){

}
