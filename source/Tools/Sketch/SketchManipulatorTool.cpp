#include "SketchManipulatorTool.h"
#include <QGraphicsScene>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

#include "Model.h"
#include "SketchView.h"
#include "GeometryHelper.h"
#include "Camera.h"

static QString ManipulationOpTitle[] = {"Move", "Rotate", "Scale X", "Scale Y", "Scale"};

SketchManipulatorTool::SketchManipulatorTool(QGraphicsItem *parent) : QGraphicsObject(parent),
    rect(QRect(0, 0, 200, 200)), manOp(TRANSLATE), leftButtonDown(false), model(nullptr)
{
    this->setAcceptHoverEvents(true);
    this->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
}

void SketchManipulatorTool::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // Messages
	if (false)
    {
        int x = 15, y = 45;
        for (auto message : messages){
            painter->drawText(x, y, message);
            y += 12;
        }
    }

    auto r = boundingRect();
    auto w = r.height() * 0.5;
    auto v = w * 0.25;

    // Translation arrows
    {
        QPolygonF arrowheadUp, arrowheadRight;

        arrowheadUp << (r.center());
        arrowheadUp << (r.center() + QPointF(v*0.25,0));
        arrowheadUp << (r.center() - QPointF(0,v));
        arrowheadUp << (r.center() - QPointF(v*0.25,0));
        for(QPointF & p : arrowheadUp) p.setY(p.y() - w + v);
        painter->setBrush(Qt::red);
        painter->setPen(QPen(Qt::red,1));
        painter->drawLine(r.center(), arrowheadUp.front());
        painter->drawPolygon(arrowheadUp);

        QTransform t;
        t.translate(r.width(),0); t.rotate(90);
        for(QPointF p : arrowheadUp) arrowheadRight << t.map(p);
        painter->setBrush(Qt::green);
        painter->setPen(QPen(Qt::green,1));
        painter->drawLine(r.center(), arrowheadRight.front());
		painter->drawPolygon(arrowheadRight);
    }

    // Rotation arc
    {
        int startAngle = 0 * 16;
        int spanAngle = 90 * 16;
        auto arcRect = r;
        arcRect.adjust(w*0.5,w*0.5,-w*0.5,-w*0.5);
        painter->setPen(QPen(QColor(0,0,255,128),v*0.5));
        painter->drawArc(arcRect,startAngle,spanAngle);

        // Visualize manipulation
        if(leftButtonDown && manOp == ROTATE)
        {
            double delta = (mouseMoveCursorPos-buttonDownCursorPos).manhattanLength();

            if(mouseMoveCursorPos.x() < buttonDownCursorPos.x()
                    || mouseMoveCursorPos.y() < buttonDownCursorPos.y()) delta *= -1;

            delta *= 3.0;

            painter->setPen(QPen(QColor(0,0,255),5));
			painter->drawArc(arcRect, startAngle - delta, spanAngle);

			curDelta = delta;
        }
    }

    // Scale buttons
    {
        QRectF scaleButtonRect(0,0,v*0.5,v*0.5);
        painter->setOpacity(0.5);

        auto sb1 = scaleButtonRect;
        sb1.moveCenter(r.center() + QPointF(0,v));
        painter->setPen(QPen(Qt::red,1));
        painter->setBrush(QColor(255,0,0,80));
        painter->fillRect(sb1,QColor(255,0,0,80));
        painter->drawRect(sb1);

        auto sb2 = scaleButtonRect;
        sb2.moveCenter(r.center() - QPointF(v,0));
        painter->setPen(QPen(Qt::green,1));
        painter->setBrush(QColor(0,255,0,80));
        painter->fillRect(sb2,QColor(0,255,0,80));
        painter->drawRect(sb2);

        auto sb3 = scaleButtonRect;
        sb3.moveCenter(r.center() - QPointF(v,-v));
        painter->setPen(QPen(Qt::yellow,1));
        painter->setBrush(QColor(255,255,0,80));
        painter->fillRect(sb3,QColor(255,255,0,80));
        painter->drawRect(sb3);

        painter->setOpacity(1.0);

        if(leftButtonDown)
        {
            double delta = (0.05 * (mouseMoveCursorPos-buttonDownCursorPos).manhattanLength());

            if(manOp == SCALE_X)
            {
                if(mouseMoveCursorPos.y() < buttonDownCursorPos.y()) delta *= -1;

                sb1.adjust(-20,-20,20,20);
                sb1.adjust(0,-delta,0,delta);
                painter->setPen(QPen(Qt::red,1));
                painter->setBrush(QColor(255,0,0,80));
                painter->fillRect(sb1,QColor(255,0,0,80));
                painter->drawRect(sb1);
            }

            if(manOp == SCALE_Y)
            {
                if(mouseMoveCursorPos.x() < buttonDownCursorPos.x()) delta *= -1;

                sb2.adjust(-20,-20,20,20);
                sb2.adjust(-delta,0,delta,0);
                painter->setPen(QPen(Qt::green,1));
                painter->setBrush(QColor(0,255,0,80));
                painter->fillRect(sb2,QColor(0,255,0,80));
                painter->drawRect(sb2);
            }

            if(manOp == SCALE_XY)
            {
                if(mouseMoveCursorPos.x() < buttonDownCursorPos.x()
                        || mouseMoveCursorPos.y() < buttonDownCursorPos.y()) delta *= -1;

                sb3.adjust(-20,-20,20,20);
                sb3.adjust(-delta,-delta,delta,delta);
                painter->setPen(QPen(Qt::yellow,1));
                painter->setBrush(QColor(255,255,0,80));
                painter->fillRect(sb3,QColor(255,255,0,80));
                painter->drawRect(sb3);
            }

			curDelta = delta;
        }
    }

    // DEBUG:
    if(false && leftButtonDown)
    {
        painter->setPen(QPen(Qt::black,1));
        painter->drawText(QPointF(21,21) - r.center(), ManipulationOpTitle[manOp]);
        painter->setPen(QPen(Qt::white,1));
		painter->drawText(QPointF(20, 20) - r.center(), ManipulationOpTitle[manOp]);
    }
}

void SketchManipulatorTool::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
	mouseMoveCursorPos = event->scenePos();

    if ((event->buttons() & Qt::LeftButton))
    {
        if (messages.size() != 2) { messages.clear(); messages << "" << ""; }

        messages[0] = QString("start mouse pos: %1, %2").arg(buttonDownCursorPos.x()).arg(buttonDownCursorPos.y());
        messages[1] = QString("cur mouse pos: %1, %2").arg(mouseMoveCursorPos.x()).arg(mouseMoveCursorPos.y());
    }

    if(manOp == TRANSLATE) QGraphicsObject::mouseMoveEvent(event);

	if (leftButtonDown)
	{
		auto p0 = view->screenToWorld(buttonDownCursorPos);
		auto p1 = view->screenToWorld(mouseMoveCursorPos);

		transform.setToIdentity();

		if (manOp == TRANSLATE)
		{
			transform.translate(p1 - p0);
		}

		if (manOp == ROTATE)
		{
			transform.rotate(curDelta, toQVector3D(view->sketchPlane->normal()));
		}

		QVector3D s(1, 1, 1);

		if (manOp == SCALE_X)
		{
			transform.scale(s + (p0 - p1));
		}

		if (manOp == SCALE_Y)
		{
			transform.scale(s + (p0 - p1));
		}

		if (manOp == SCALE_XY)
		{
            transform.scale(s * curDelta * 0.05f);
		}

        emit(transformChange(transform));
        if(model != nullptr) model->transformActiveNodeGeometry(transform);
	}

	scene()->update(sceneBoundingRect());
}

void SketchManipulatorTool::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
	if (model != nullptr) model->storeActiveNodeGeometry();

    if (event->buttons() & Qt::LeftButton) leftButtonDown = true;
    if (event->buttons() & Qt::RightButton) rightButtonDown = true;
    if (event->buttons() & Qt::MiddleButton) middleButtonDown = true;

    manOp = ManipulationOp::TRANSLATE;

	// Record mouse states
	buttonDownCursorPos = event->scenePos();
	mouseMoveCursorPos = event->scenePos();

    auto r = boundingRect();
    auto w = r.height() * 0.5;
    auto v = w * 0.25;

    // Rotation
    {
        auto arcRect = r;
        arcRect.adjust(w*0.5,w*0.5,-w*0.5,-w*0.5);
        auto rotRect = QRectF(0,0,arcRect.width()*0.5, arcRect.height()*0.5);
        rotRect.moveBottomLeft(r.center());

        if(rotRect.contains(event->pos())) manOp = ROTATE;
    }

    // Scale
    {
        QRectF scaleButtonRect(0,0,v*0.5,v*0.5);

        auto sb1 = scaleButtonRect;
        sb1.moveCenter(r.center() + QPointF(0,v));

        auto sb2 = scaleButtonRect;
        sb2.moveCenter(r.center() - QPointF(v,0));

        auto sb3 = scaleButtonRect;
        sb3.moveCenter(r.center() - QPointF(v,-v));

        if(sb1.contains(event->pos())) manOp = SCALE_X;
        if(sb2.contains(event->pos())) manOp = SCALE_Y;
        if(sb3.contains(event->pos())) manOp = SCALE_XY;
    }

	if (manOp == TRANSLATE) QGraphicsObject::mousePressEvent(event);

	scene()->update(sceneBoundingRect());
}

void SketchManipulatorTool::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
    // Record mouse states
    buttonUpCursorPos = event->pos();

    manOp = TRANSLATE;

    QGraphicsObject::mouseReleaseEvent(event);

    leftButtonDown = false;
    rightButtonDown = false;

	scene()->update(sceneBoundingRect());
}

void SketchManipulatorTool::setRect(const QRectF & newRect)
{
    this->rect = newRect;
}
