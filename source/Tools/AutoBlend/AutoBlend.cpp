#include "AutoBlend.h"
#include "ui_AutoBlend.h"

#include "Document.h"
#include "Model.h"
#include "Camera.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"
#include "Gallery.h"
#include "Thumbnail.h"

#include <QGraphicsDropShadowEffect>
#include <QTimer>

#include "GraphCorresponder.h"
#include "TopoBlender.h"
#include "Scheduler.h"
#include "SynthesisManager.h"

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

AutoBlend::AutoBlend(Document *document, const QRectF &bounds) : Tool(document), gallery(nullptr), results(nullptr)
{
    setBounds(bounds);
    setObjectName("autoBlend");
}

void AutoBlend::init()
{
    // Add widget
    auto toolsWidgetContainer = new QWidget();
    widget = new Ui::AutoBlendWidget();
    widget->setupUi(toolsWidgetContainer);
    widgetProxy = new QGraphicsProxyWidget(this);
    widgetProxy->setWidget(toolsWidgetContainer);

    // Place at bottom left corner
    auto delta = widgetProxy->sceneBoundingRect().bottomLeft() -
    scene()->views().front()->rect().bottomLeft();
    widgetProxy->moveBy(-delta.x(), -delta.y());

    // Fill categories box
    {
        for(auto cat : document->categories.keys()){
            widget->categoriesBox->insertItem(widget->categoriesBox->count(), cat);
        }

        int idx = widget->categoriesBox->findText(document->categoryOf(document->firstModelName()));
        widget->categoriesBox->setCurrentIndex(idx);
    }

    // Create gallery of shapes
    gallery = new Gallery(this, QRectF(0,0,this->bounds.width(), 180));

    // Create container that holds results
    results = new Gallery(this, QRectF(0,0, this->bounds.width(),
                          bounds.height() - gallery->boundingRect().height()),
                          QRectF(0,0,256,256), true);

    results->moveBy(0, gallery->boundingRect().height());

    // Gallery on top of results
    gallery->setZValue(results->zValue() + 10);

    auto dropShadow = new QGraphicsDropShadowEffect();
	dropShadow->setOffset(0, 5);
	dropShadow->setColor(Qt::black);
	dropShadow->setBlurRadius(10);
	gallery->setGraphicsEffect(dropShadow);

	// Connect UI with actions
	{
		connect(widget->categoriesBox, &QComboBox::currentTextChanged, [&](QString text){
			document->currentCategory = text;
		});

		connect(widget->analyzeButton, &QPushButton::pressed, [&](){
			document->analyze(widget->categoriesBox->currentText());
		});

		// Default view angle
		{
			// Camera target and initial position
			auto camera = new Eigen::Camera();
			auto frame = camera->frame();
			frame.position = Eigen::Vector3f(-1, 0, 0.5);
			camera->setTarget(Eigen::Vector3f(0, 0, 0.5));
            camera->setFrame(frame);

            int deltaZoom = document->extent().length() * 1.0;

            // Default view angle
            double theta1 = acos(-1) * 0.75;
            double theta2 = acos(-1) * 0.10;
            camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
            camera->zoom(-(4+deltaZoom));
            camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta2, Eigen::Vector3f::UnitX())));
			auto cp = camera->position();
			cameraPos = QVector3D(cp[0], cp[1], cp[2]);

			// Camera settings
			camera->setViewport(128, 128);
			Eigen::Matrix4f p = camera->projectionMatrix();
			Eigen::Matrix4f v = camera->viewMatrix().matrix();
			p.transposeInPlace();
			v.transposeInPlace();
			cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());
        }

		connect(document, &Document::categoryAnalysisDone, [=](){
			if (gallery == nullptr) return;

			// Fill gallery
			gallery->clearThumbnails();

			auto catModels = document->categories[document->currentCategory].toStringList();
			for (auto targetName : catModels)
			{
				auto targetModel = document->cacheModel(targetName);
				auto t = gallery->addTextItem(targetName);

				QVariantMap data = t->data;
				data["targetName"].setValue(targetName);
				t->setData(data);

				t->setCamera(cameraPos, cameraMatrix);
				t->setFlag(QGraphicsItem::ItemIsSelectable);

				// Add parts of target shape
				for (auto n : targetModel->nodes){
					t->addAuxMesh(toBasicMesh(targetModel->getMesh(n->id), n->vis_property["color"].value<QColor>()));
				}

                scene()->update(t->sceneBoundingRect());
            }

            scene()->update(this->sceneBoundingRect());
            QTimer::singleShot(500, [=]{ gallery->update(); });
		});

		// Do blend
		connect(widget->blendButton, SIGNAL(pressed()), SLOT(doBlend())); 
	}
}

void AutoBlend::doBlend()
{
	auto selected = gallery->getSelected();
	if (selected.size() < 2) return;

    for(auto t : results->items) t->deleteLater();
    results->items.clear();

    ((GraphicsScene*)scene())->showPopup("Please wait..");

    for(int shapeI = 0; shapeI < selected.size(); shapeI++)
    {
        for(int shapeJ = shapeI + 1; shapeJ < selected.size(); shapeJ++)
        {
            //auto sourceName = selected.front()->data["targetName"].toString();
            //auto targetName = selected.back()->data["targetName"].toString();

            auto sourceName = selected[shapeI]->data.value("targetName").toString();
            auto targetName = selected[shapeJ]->data.value("targetName").toString();

            auto cacheSource = document->cacheModel(sourceName);
            auto cacheTarget = document->cacheModel(targetName);

            if(cacheSource == nullptr || cacheTarget == nullptr) continue;

            auto source = QSharedPointer<Structure::Graph>(cacheSource->cloneAsShapeGraph());
			auto target = QSharedPointer<Structure::Graph>(cacheTarget->cloneAsShapeGraph());

			auto gcorr = QSharedPointer<GraphCorresponder>(new GraphCorresponder(source.data(), target.data()));

			// Apply computed correspondence
			if (false) // enable/disable auto correspondence
			{
				for (auto n : source->nodes)
				{
					QString sid = n->id;

					if (document->datasetCorr[sourceName][sid][targetName].empty())
					{
						gcorr->setNonCorresSource(sid);
					}
					else
					{
						auto tid = document->datasetCorr[sourceName][sid][targetName].front();
						gcorr->addLandmarks(QVector<QString>() << sid, QVector<QString>() << tid);
					}
				}
			}

            gcorr->computeCorrespondences();

            // Schedule blending sequence
            auto scheduler = QSharedPointer<Scheduler>(new Scheduler);
            auto blender = QSharedPointer<TopoBlender>(new TopoBlender(gcorr.data(), scheduler.data()));

            // Sample geometries
            int numSamples = 100;
            int reconLevel = 4;

            int LOD = widget->levelDetails->currentIndex();
            switch (LOD){
                case 0: numSamples = 100; reconLevel = 4; break;
                case 1: numSamples = 1000; reconLevel = 5; break;
                case 3: numSamples = 10000; reconLevel = 7; break;
            }

            auto synthManager = QSharedPointer<SynthesisManager>(new SynthesisManager(gcorr.data(), scheduler.data(), blender.data(), numSamples));
            synthManager->genSynData();

            // Compute blending
            scheduler->timeStep = 1.0 / 100.0;
            scheduler->defaultSchedule();
            scheduler->executeAll();

            int numResults = widget->count->value();

            for (int i = 0; i < numResults; i++)
            {
                double a = ((double(i) / (numResults - 1)) * 0.6) + 0.2;
                auto blendedModel = scheduler->allGraphs[a * (scheduler->allGraphs.size() - 1)];

                synthManager->renderGraph(*blendedModel, "", false, reconLevel );

                auto t = results->addTextItem("");
                t->setCamera(cameraPos, cameraMatrix);

                // Add parts of target shape
                for (auto n : blendedModel->nodes){
                    t->addAuxMesh(toBasicMesh(blendedModel->getMesh(n->id), n->vis_property["color"].value<QColor>()));
                }
            }
        }
    }

    ((GraphicsScene*)scene())->hidePopup();
}
