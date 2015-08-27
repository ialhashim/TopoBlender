#pragma once
#include <QGraphicsObject>
#include <QVector3D>
#include <QMatrix4x4>

class Thumbnail : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QRectF rect READ boundingRect WRITE setRect)

public:
    Thumbnail(QGraphicsItem *parent, QRectF rect = QRectF(0,0,128,128));

    QRectF rect;
    QRectF boundingRect() const { return rect; }
    void setRect(const QRectF & newRect){ rect = newRect; }

    void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget = 0);
    void prePaint(QPainter * painter, QWidget * widget);
    void postPaint(QPainter * painter, QWidget * widget);

    struct QBasicMesh{
		QVector<QVector3D> points, normals; QColor color;
		void addTri(QVector3D v0, QVector3D v1, QVector3D v2,
			QVector3D n0, QVector3D n1, QVector3D n2){
			points << v0 << v1 << v2;
			normals << n0 << n1 << n2;
		}
	};

    QVariantMap data;

	void setCamera(QVector3D eyePos, QMatrix4x4 camera){ eye = eyePos; pvm = camera; }
	QMatrix4x4 pvm;
	QVector3D eye;

public:
    void setImage(QImage image){ img = image.width() <= rect.width() ?
                    image : image.scaledToWidth(rect.width(), Qt::SmoothTransformation); }
    void setCaption(QString text) { caption = text; }
	void setMesh(QBasicMesh shape) { mesh = shape; }
    void set(QImage image, QString text, QBasicMesh shape){
        setImage(image);setCaption(text);setMesh(shape);
    }

    void setData(QVariantMap fromData){ data = fromData; }

	void addAuxMesh(QBasicMesh auxMesh){ auxMeshes << auxMesh; }

public:
    static QBasicMesh buildTetrahedron(float length);

protected:
    QImage img;
    QString caption;
    QBasicMesh mesh;
	QImage meshImage;
	QVector<QBasicMesh> auxMeshes;

	void mousePressEvent(QGraphicsSceneMouseEvent * event);

signals:
	void clicked(Thumbnail*);
};
