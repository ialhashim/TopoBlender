#include "ManualBlendView.h"

#include "GraphicsView.h"
#include "GraphicsScene.h"
#include "Viewer.h"
#include "Camera.h"

#include "Document.h"
#include "Model.h"

#include "Thumbnail.h"
#include "Gallery.h"

#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>

ManualBlendView::ManualBlendView(Document *document, QGraphicsItem * parent) : QGraphicsObject(parent), document(document), gallery(nullptr)
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

        // Default view angle
        double theta1 = acos(-1) * 0.75;
        double theta2 = acos(-1) * 0.10;
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
        camera->zoom(-4);
        camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));

        trackball->setCamera(camera);
    }
}

ManualBlendView::~ManualBlendView()
{
    delete camera; delete trackball;
}

void ManualBlendView::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget * widget)
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

		// Draw intermediate geometry
		{
			if (!cloudPoints.empty())
			{
				glwidget->glPointSize(8);
				glwidget->drawOrientedPoints(cloudPoints, cloudNormals, cloudColor, cameraMatrix);
			}
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

void ManualBlendView::prePaint(QPainter *painter, QWidget *)
{
    // Background
    auto lightBackColor = QColor::fromRgb(124, 143, 162);
    auto darkBackColor = QColor::fromRgb(27, 30, 32);

    QLinearGradient gradient(0, 0, 0, rect.height());
    gradient.setColorAt(0.0, lightBackColor);
    gradient.setColorAt(1.0, darkBackColor);
    painter->fillRect(rect, gradient);
}

void ManualBlendView::postPaint(QPainter *, QWidget *)
{
    // Border
    //painter->setPen(QPen(Qt::gray, 10));
    //painter->drawRect(rect);
}

void ManualBlendView::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
    mouseMoveCursorPos = event->pos();

    if(rightButtonDown)
    {
        trackball->track(Eigen::Vector2i(mouseMoveCursorPos.x(), mouseMoveCursorPos.y()));
    }

    scene()->update(sceneBoundingRect());
}

void ManualBlendView::mousePressEvent(QGraphicsSceneMouseEvent * event)
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
		// Convert click to ray
		int screenWidth = rect.width();
		int screenHeight = rect.height();
		camera->setViewport(screenWidth, screenHeight);
		Eigen::Vector3f orig = camera->position();
		Eigen::Vector3f dir = camera->unProject(Eigen::Vector2f(event->pos().x(), screenHeight - event->pos().y()), 10) - orig;
		dir.normalize();
		auto rayOrigin = QVector3D(orig[0],orig[1],orig[2]);
		auto rayDirection = QVector3D(dir[0], dir[1], dir[2]);

		// Select part
		document->selectPart(document->firstModelName(), rayOrigin, rayDirection);

		if (document->getModel(document->firstModelName())->activeNode == nullptr)
		{
			// Clear previous suggestions
			if(gallery) delete gallery;
			gallery = nullptr;
		}
		else
		{
			// Show suggestions to user
			suggestParts(event->pos());
		}
	}

    scene()->update(sceneBoundingRect());
}

void ManualBlendView::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    this->setFocus();

	leftButtonDown = false;
	rightButtonDown = false;
	middleButtonDown = false;

    scene()->update(sceneBoundingRect());
}

void ManualBlendView::wheelEvent(QGraphicsSceneWheelEvent * event)
{
    QGraphicsObject::wheelEvent(event);
    double step = (double)event->delta() / 120;
    camera->zoom(step);

    scene()->update(sceneBoundingRect());
}

void ManualBlendView::keyPressEvent(QKeyEvent *){

}

void ManualBlendView::suggestParts(QPointF galleryPos)
{
	auto model = document->getModel(document->firstModelName());
	if (model == nullptr || model->activeNode == nullptr) return;

    QString sourceName = document->firstModelName();
	QString sourcePart = model->activeNode->id;
    if (document->datasetCorr[sourceName][sourcePart].empty()){
		((GraphicsScene*)scene())->displayMessage("No correspondence found", 500);
		return;
	}

	// Create gallery of suggestions as thumbnails
	int numCol = 3, numRow = 2;
	QRectF thumbRect(0, 0, 128, 128);
	QRectF galleryRect(0, 0, thumbRect.width() * numCol, thumbRect.height() * numRow);
	
	if (gallery) gallery->deleteLater();
    gallery = new Gallery(this, galleryRect, thumbRect);

	// View settings
	camera->setViewport(thumbRect.width(), thumbRect.height()); // modify
	Eigen::Matrix4f p = camera->projectionMatrix();
	Eigen::Matrix4f v = camera->viewMatrix().matrix();
	p.transposeInPlace();v.transposeInPlace();
	QMatrix4x4 viewMatrix(v.data());
	QMatrix4x4 projectionMatrix(p.data());
	auto eyePos = camera->position();
	QMatrix4x4 thumb_pvm = projectionMatrix * viewMatrix;
	QVector3D thumb_eye = QVector3D(eyePos[0], eyePos[1], eyePos[2]);
	camera->setViewport(rect.width(), rect.height()); // restore

	QVariantMap sharedData;
	sharedData["sourcePart"].setValue(sourcePart);

	auto toBasicMesh = [&](opengp::SurfaceMesh::SurfaceMeshModel * m, QColor color){
		Thumbnail::QBasicMesh mesh;
		m->update_face_normals();
		for (auto f : m->faces()){
			QVector<QVector3D> fp, fn;
			for (auto vf : m->vertices(f)){
				auto p = m->vertex_coordinates()[vf];
				auto n = m->face_normals()[f];
				fp << QVector3D(p[0], p[1], p[2]);
				fn << QVector3D(n[0], n[1], n[2]);
			}
			mesh.addTri(fp[0], fp[1], fp[2], fn[0], fn[1], fn[2]);
		}
		mesh.color = color;
		return mesh;
	};

    for (auto targetName : document->datasetCorr[sourceName][sourcePart].keys())
	{
		auto targetModel = document->cacheModel(targetName);

        for (auto targetPartName : document->datasetCorr[sourceName][sourcePart][targetName])
		{
			auto data = sharedData;

			data["targetName"].setValue(targetName);
			data["targetPartName"].setValue(targetPartName);

			auto targetPartMesh = targetModel->getMesh(targetPartName);

			auto t = gallery->addMeshItem(toBasicMesh(targetPartMesh, Qt::yellow), data);

			t->setCamera(thumb_eye, thumb_pvm);

			// Add remaining parts of target shape
			for (auto n : targetModel->nodes){
				if (n->id == targetPartName) continue;
				t->addAuxMesh(toBasicMesh(targetModel->getMesh(n->id), QColor(128,128,128,128)));
			}

			connect(t, SIGNAL(clicked(Thumbnail *)), SLOT(suggestionClicked(Thumbnail *)));
		}
	}

	gallery->setPos(galleryPos);
}

void ManualBlendView::suggestionClicked(Thumbnail * t)
{
    //((GraphicsScene*)scene())->displayMessage("bingo!", 500);

	// Place exactly where it was
	auto gallpos = gallery->pos();
	t->setParentItem(this);
	t->moveBy(gallpos.x(), gallpos.y());

	// Remove the connection to this
	t->disconnect(this, SLOT(suggestionClicked(Thumbnail *)));

	// Get blend info
	QString sourcePartName = t->data["sourcePart"].toString();
	QString targetName = t->data["targetName"].toString();
	QString targetPartName = t->data["targetPartName"].toString();

	// Add blending slider & buttons
	{
		auto blendControlsWidget = new QWidget;
		auto blendControlsWidgetLayout = new QVBoxLayout;

		auto blendSlider = new QSlider(Qt::Horizontal);
		blendSlider->setRange(0, 100);
		blendControlsWidgetLayout->addWidget(blendSlider);

		auto blendAccept = new QPushButton("Accept");
		blendControlsWidgetLayout->addWidget(blendAccept);
		blendControlsWidget->setLayout(blendControlsWidgetLayout);
		blendControlsWidget->setStyleSheet(".QWidget{ background:transparent; }");

		blendControlsWidget->setMinimumWidth(t->boundingRect().width());
		blendControlsWidget->setMaximumWidth(t->boundingRect().width());

		auto blendControlsProxy = new QGraphicsProxyWidget(t);
		blendControlsProxy->setWidget(blendControlsWidget);

		blendControlsProxy->setPos(0,t->rect.height());

		// Interaction
		connect(blendSlider, &QSlider::valueChanged, [=](int value){
			emit( doBlend(sourcePartName, targetName, targetPartName, value) );
		});

		t->connect(blendAccept, &QPushButton::pressed, [=](){
			emit( finalizeBlend(sourcePartName, targetName, targetPartName, blendSlider->value()) );
			cloudPoints.clear();
			cloudNormals.clear();
			t->deleteLater();
		});

        blendSlider->setValue(50);
	}

	// Close gallery
	delete gallery;
	gallery = nullptr;
}

void ManualBlendView::cloudReceived(QPair< QVector<Eigen::Vector3f>, QVector<Eigen::Vector3f> > cloud)
{
    cloudPoints.clear();
    cloudNormals.clear();

    auto source = document->getModel(document->firstModelName());
    auto n = source->activeNode;

    // Check if blending is canceld
    if(cloud.first.empty()){
        n->vis_property["isHidden"].setValue(false);

        // remaining elements of a group
        for(auto nj : source->nodes){
            if(nj == n || !source->shareGroup(nj->id, n->id)) continue;
            nj->vis_property["isHidden"].setValue(false);
        }
        return;
    }

	for (auto p : cloud.first) cloudPoints << QVector3D(p[0],p[1],p[2]);
	for (auto p : cloud.second) cloudNormals << QVector3D(p[0], p[1], p[2]);

	n->vis_property["isHidden"].setValue(true);
	cloudColor = n->vis_property["color"].value<QColor>();

    // remaining elements of a group
    for(auto nj : source->nodes){
        if(nj == n || !source->shareGroup(nj->id, n->id)) continue;
        nj->vis_property["isHidden"].setValue(true);
    }
}
