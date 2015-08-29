#include "Thumbnail.h"
#include <QPainter>

#include "GraphicsView.h"
#include "Viewer.h"

#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>

Thumbnail::Thumbnail(QGraphicsItem *parent, QRectF rect) : QGraphicsObject(parent), rect(rect)
{
	//setFlags( QGraphicsItem::ItemIsMovable );
	setFlags(QGraphicsItem::ItemIsFocusable);

    // Default values
    setImage(QImage(":/icons/thumbnail.png"));
    setCaption("thumbnail");
    setMesh(Thumbnail::buildTetrahedron(1.0));

    // Show default image until something changes it
    isTempImage = true;
}

void Thumbnail::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
	QRectF parentRect = parentItem()->sceneBoundingRect();

	// Skip drawing thumbnails outside their parents
	if (!sceneBoundingRect().intersects(parentRect)) return;

    prePaint(painter, widget);

    // Draw image
    if(!img.isNull())
    {
        auto imgRect = QRectF(img.rect());
        imgRect.moveCenter(rect.center());
        painter->drawImage(imgRect.topLeft(), img);
    }

    // Draw 3D mesh
    if(mesh.points.size() || auxMeshes.size())
	{
        if (img.isNull() || isTempImage)
		{
			auto glwidget = (Viewer*)widget;
			if (glwidget)
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
				QOpenGLFramebufferObject renderFbo(rect.width() * 2, rect.height() * 2, fboformat);
				renderFbo.bind();

				glwidget->glEnable(GL_DEPTH_TEST);
				glwidget->glEnable(GL_BLEND);
				glwidget->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glwidget->glCullFace(GL_BACK);

				glwidget->glClearColor(0,0,0,0);
				glwidget->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glwidget->glViewport(0, 0, renderFbo.size().width(), renderFbo.size().height());
				
                if(mesh.points.size())
                    glwidget->drawTriangles(mesh.color, mesh.points, mesh.normals, pvm);

				// Draw aux meshes
				for (auto auxmesh : auxMeshes)
					glwidget->drawTriangles(auxmesh.color, auxmesh.points, auxmesh.normals, pvm);
				
				glwidget->glDisable(GL_DEPTH_TEST);
				glwidget->glFlush();

				renderFbo.release();
                this->setImage( renderFbo.toImage().scaledToWidth(rect.width(), Qt::SmoothTransformation) );

				// Thanks for sharing!
				glwidget->makeCurrent();
			}

			/*painter->beginNativePainting();

			auto glwidget = (Viewer*)widget;
			if (glwidget)
			{
				// Draw mesh
				auto r = sceneBoundingRect();
				auto v = scene()->views().first();
				QPoint viewDelta = v->mapFromScene(r.topLeft());
				if (viewDelta.manhattanLength() > 5) r.moveTopLeft(viewDelta);

				glwidget->eyePos = eye;
				glwidget->pvm = pvm;
				glwidget->glViewport(r.left(), v->height() - r.height() - r.top(), r.width(), r.height());

				// Clipping OpenGL
				glwidget->glEnable(GL_SCISSOR_TEST);
				glwidget->glScissor(parentRect.x(), v->height() - parentRect.height() - parentRect.top(), parentRect.width(), parentRect.height());

				glwidget->glClear(GL_DEPTH_BUFFER_BIT);
				glwidget->drawTriangles(mesh.color, mesh.points, mesh.normals, pvm);

				// Draw aux meshes
				for (auto auxmesh : auxMeshes)
				{
					glwidget->drawTriangles(auxmesh.color, auxmesh.points, auxmesh.normals, pvm);
				}

				glwidget->glDisable(GL_SCISSOR_TEST);
			}

			painter->endNativePainting();*/
		}        
    }
    // Draw the caption
    if(caption.size())
    {
        painter->setPen(QPen(Qt::white,1));
        auto textRect = rect;
        textRect.setHeight(painter->fontMetrics().height() * 1.25);
        textRect.moveBottom(rect.height() - textRect.height() * 0.5);
        painter->drawText(textRect, caption, Qt::AlignVCenter | Qt::AlignHCenter);
    }

    postPaint(painter, widget);
}

void Thumbnail::prePaint(QPainter *painter, QWidget *)
{
    bool isFlatBackground = false;

    // Flat Background
    if(isFlatBackground)
    {
        QPainterPath backRect;
        auto r = rect.width() * 0.1;
        backRect.addRoundedRect(rect, r, r);

        painter->setPen(QPen(Qt::transparent,0));
        painter->setBrush(QColor(QColor(0,0,0,128)));
        painter->drawPath(backRect);
    }
    else
    // Radial gradient background
    {
        QRadialGradient grad(rect.center(), rect.width() * 0.5);
        grad.setColorAt(0, QColor(0,0,0,255));
        grad.setColorAt(1, Qt::transparent);
        painter->fillRect(rect, grad);
    }
}

void Thumbnail::postPaint(QPainter * painter, QWidget *)
{
	if (this->isUnderMouse()){
		painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(QPen(QColor(255,255,0,128), 1));
        painter->drawRect(rect.adjusted(1,1,-1,-1));
	}

    if (this->isSelected()){
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(QPen(Qt::yellow, 4));
        painter->drawRect(rect);
    }
}

Thumbnail::QBasicMesh Thumbnail::buildTetrahedron(float length)
{
    Thumbnail::QBasicMesh m;
    m.color = Qt::blue;
    static QVector3D vertices[] = {
        QVector3D( 1.0,  1.0,  1.0), // V0
        QVector3D(-1.0,  1.0, -1.0), // V1
        QVector3D( 1.0, -1.0, -1.0), // V2
        QVector3D(-1.0, -1.0,  1.0)  // v3
    };
    static int indices[4][3] ={
        {1, 2, 3}, // F0
        {0, 3, 2}, // F1
        {0, 1, 3}, // F2
        {0, 2, 1}  // F3
    };
    for(int f = 0; f < 4; f++){
        for(int v = 0; v < 3; v++){
            m.points.push_back( QVector3D(vertices[indices[f][v]] * length) );
        }
    }
    for(int f = 0; f < 4; f++){
        QVector3D fn = QVector3D::crossProduct(
                    vertices[indices[f][1]] - vertices[indices[f][0]],
                    vertices[indices[f][2]] - vertices[indices[f][0]]).normalized();
        m.normals.push_back(fn);
        m.normals.push_back(fn);
        m.normals.push_back(fn);
    }
    return m;
}

void Thumbnail::mousePressEvent(QGraphicsSceneMouseEvent *)
{
    emit(clicked(this));
}

void Thumbnail::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    emit(doubleClicked(this));
}
