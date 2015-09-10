#pragma once

#include <QGraphicsObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <Eigen/Core>

class Document;
class Gallery;
class Thumbnail;

namespace Eigen{ class Camera; class Trackball; class Plane; }

class ManualBlendView : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    ManualBlendView(Document * document, QGraphicsItem *parent);
    ~ManualBlendView();

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
    void prePaint(QPainter * painter, QWidget * widget);
    void postPaint(QPainter * painter, QWidget * widget);

    QRectF rect;
    QRectF boundingRect() const { return this->rect; }
    void setRect(const QRectF & newRect){ this->rect = newRect; }

public slots:
	void suggestParts(QPointF galleryPos);
	void suggestionClicked(Thumbnail * t);

	void cloudReceived(QPair< QVector<Eigen::Vector3f>, QVector<Eigen::Vector3f> >);

protected:
    Document * document;
	Gallery * gallery;

    // Camera movement
    Eigen::Camera* camera;
    Eigen::Trackball* trackball;

	// Intermediate geometry
	QVector<QVector3D> cloudPoints;
	QVector<QVector3D> cloudNormals;
	QColor cloudColor;

    // Options
    QVariantMap options;

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *);
    void mousePressEvent(QGraphicsSceneMouseEvent *);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
    void wheelEvent(QGraphicsSceneWheelEvent *);
    QPointF buttonDownCursorPos, mouseMoveCursorPos, buttonUpCursorPos;
    bool leftButtonDown, rightButtonDown, middleButtonDown;

    void keyPressEvent(QKeyEvent * event);

signals:
	void doBlend(QString sourcePartName, QString targetName, QString targetPartName, int value);
	void finalizeBlend(QString sourcePartName, QString targetName, QString targetPartName, int value);
};
