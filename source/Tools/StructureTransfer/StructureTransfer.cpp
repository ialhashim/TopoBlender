#include "StructureTransfer.h"
#include <QWidget>
#include <QSettings>
#include "ui_StructureTransfer.h"
#include "StructureTransferView.h"

#include "Document.h"
#include "Model.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"
#include "Camera.h"
#include "Gallery.h"

#include <QGraphicsDropShadowEffect>
#include <QTimer>

#include <QProcess>
#include <QTemporaryFile>

Q_DECLARE_METATYPE(Array2D_Vector3)

#include "EnergyGuidedDeformation.h"
#include "EncodeDecodeGeometry.h"

#include "IsotropicRemesher.h"

StructureTransfer::StructureTransfer(Document *document, const QRectF &bounds) : Tool(document), view(nullptr)
{
    connect(this, SIGNAL(boundsChanged()), SLOT(resizeViews()));

    setBounds(bounds);
    setObjectName("structureTransfer");
}

void StructureTransfer::init()
{
    view = new StructureTransferView(document, this);

    // Add widget
    auto toolsWidgetContainer = new QWidget();
    widget = new Ui::StructureTransferWidget();
    widget->setupUi(toolsWidgetContainer);
    widgetProxy = new QGraphicsProxyWidget(this);
    widgetProxy->setWidget(toolsWidgetContainer);

    // Place at bottom left corner
    auto delta = widgetProxy->sceneBoundingRect().bottomLeft() -
    scene()->views().front()->rect().bottomLeft();
    widgetProxy->moveBy(-delta.x(), -delta.y());

    // Create gallery of shapes
    gallery = new Gallery(this, QRectF(0,0,this->bounds.width(), 180));

    auto dropShadow = new QGraphicsDropShadowEffect();
    dropShadow->setOffset(0, 5);
    dropShadow->setColor(QColor(0,0,0,128));
    dropShadow->setBlurRadius(10);
    gallery->setGraphicsEffect(dropShadow);

    // Fill categories box
    {
        for(auto cat : document->categories.keys()){
            widget->categoriesBox->insertItem(widget->categoriesBox->count(), cat);
        }

        int idx = widget->categoriesBox->findText(document->categoryOf(document->firstModelName()));
        widget->categoriesBox->setCurrentIndex(idx);
    }

    // Connect UI with actions
    {
        connect(widget->categoriesBox, &QComboBox::currentTextChanged, [&](QString text){
            document->currentCategory = text;
        });

        connect(widget->analyzeButton, &QPushButton::pressed, [&](){
            document->analyze(widget->categoriesBox->currentText());
        });

		connect(widget->resampleButton, &QPushButton::pressed, [&](){
			auto sourceModel = document->getModel(document->firstModelName());

			// external re-meshing software
			if (QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier))
			{
				qApp->setOverrideCursor(Qt::WaitCursor);

				bool isGood = false;

				for (auto n : sourceModel->nodes)
				{
					auto m = sourceModel->getMesh(n->id);

					QTemporaryFile file("XXXXXX.obj");

					if (file.open()){
						m->write(file.fileName().toStdString());

						QProcess p;

						QStringList pargs;
						pargs << file.fileName();
						pargs << file.fileName();

						p.start("vorpalite.exe", pargs);
						p.waitForFinished(-1);

						m->clear();
						m->read(file.fileName().toStdString());
						m->update_face_normals();
						m->update_vertex_normals();
						m->updateBoundingBox();

						isGood = true;
					}
				}

				((GraphicsScene*)scene())->displayMessage(QString("External mesher done. OK = %1").arg(isGood));

				qApp->restoreOverrideCursor();

				return;
			}

			for (auto n : sourceModel->nodes)
			{
				auto m = sourceModel->getMesh(n->id);

				Remesh::IsotropicRemesher mesher(m);
				mesher.apply();
			}
		});
		
        connect(document, &Document::categoryAnalysisDone, [=](){
            if (gallery == nullptr) return;

            // Fill gallery
            gallery->clearThumbnails();

            QRectF thumbRect(0,0,128,128);

            // Camera settings
            QMatrix4x4 cameraMatrix;
            view->camera->setViewport(thumbRect.width(), thumbRect.height());
            Eigen::Matrix4f p = view->camera->projectionMatrix();
            Eigen::Matrix4f v = view->camera->viewMatrix().matrix();
            p.transposeInPlace();
            v.transposeInPlace();
            cameraMatrix = QMatrix4x4(p.data()) * QMatrix4x4(v.data());
            QVector3D cameraPos(view->camera->position().x(),view->camera->position().y(),view->camera->position().z());

            // From surface mesh to basic mesh
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

                QObject::connect(t, SIGNAL(doubleClicked(Thumbnail*)), this, SLOT(thumbnailSelected(Thumbnail*)));

                scene()->update(t->sceneBoundingRect());
            }

            scene()->update(this->sceneBoundingRect());
            QTimer::singleShot(500, [=]{ gallery->update(); });
        });
    }

    resizeViews();
}

void StructureTransfer::resizeViews()
{
    if(view) view->setRect(bounds);
	if (gallery){
		auto r = gallery->rect;
		r.setWidth(bounds.width());
		gallery->setRect(r);
	}
}

void StructureTransfer::thumbnailSelected(Thumbnail * t)
{
    QVariantMap data = t->data;
    auto sourceName = document->firstModelName();
    auto targetName = data["targetName"].toString();

    auto sourceModel = document->getModel(sourceName);
    auto targetModel = document->cacheModel(targetName);

	if (!sourceModel->ShapeGraph::property.contains("origPoints"))
		sourceModel->ShapeGraph::property["origPoints"].setValue(sourceModel->getAllControlPoints());
	else
		sourceModel->setAllControlPoints(sourceModel->ShapeGraph::property["origPoints"].value<Array2D_Vector3>());

    ShapeGeometry::encodeGeometry(sourceModel);

    if(document->datasetCorr.contains(sourceName))
    {
        auto shapeA = QSharedPointer<Structure::ShapeGraph>(new Structure::ShapeGraph(*sourceModel));
        auto shapeB = QSharedPointer<Structure::ShapeGraph>(new Structure::ShapeGraph(*targetModel));

        QStringList la, lb;
        QSet<QString> tempFixed;

        for (auto n : sourceModel->nodes)
        {
            n->vis_property["isHidden"].setValue(false);

            if(document->datasetCorr[sourceName][n->id].contains(targetName))
            {
                la << n->id;
                lb << document->datasetCorr[sourceName][n->id][targetName].front();
            }
            else
            {
                n->vis_property["isHidden"].setValue(true);
            }
        }

        Energy::GuidedDeformation egd;
        egd.preprocess(shapeA.data(), shapeB.data());
        egd.topologicalOpeartions(shapeA.data(), shapeB.data(), la, lb);
        egd.applyDeformation(shapeA.data(), shapeB.data(), la, lb, tempFixed, true);

        for (auto n : sourceModel->nodes)
        {
            n->setControlPoints(shapeA->getNode(n->id)->controlPoints());
        }
    }
    else
    {
        auto shapeA = QSharedPointer<Structure::ShapeGraph>(new Structure::ShapeGraph(*sourceModel));
        auto shapeB = QSharedPointer<Structure::ShapeGraph>(new Structure::ShapeGraph(*targetModel));

        QSharedPointer<Energy::GuidedDeformation> egd = QSharedPointer<Energy::GuidedDeformation>(new Energy::GuidedDeformation);

        egd->K = 20;
        egd->K_2 = 4;
        QVector<Energy::SearchNode> search_roots;
        egd->searchDP(shapeA.data(), shapeB.data(), search_roots);

        // Extract matches
        QVector< QPair<QString,QString> > pairs;
        {
            Energy::SearchNode * selected_path = &(search_roots.back());

            QSet< QString > matchings;
            for (auto key : selected_path->mapping.keys())
            {
                auto sn = selected_path->shapeA->getNode(key);
                auto tn = selected_path->shapeB->getNode(selected_path->mapping[key]);

                // For cutting case + splitting case
                auto realID = [&](Structure::Node * n){
                    QString id = n->property.contains("realOriginalID") ? n->property["realOriginalID"].toString() : n->id;
                    return id.split("@").front();
                };

                QString sid = realID(sn);
                QString tid = realID(tn);

                // Check for one-to-many case
                bool isSourceOne = !selected_path->shapeA->hasRelation(sid) || selected_path->shapeA->relationOf(sid).parts.size() == 1;
                bool isTargetMany = selected_path->shapeB->hasRelation(tid) && selected_path->shapeB->relationOf(tid).parts.size() > 1;
                if (isSourceOne && isTargetMany)
                {
                    for (auto tj : selected_path->shapeB->relationOf(tid).parts)
                        matchings << QString("%1|%2").arg(sid).arg(realID(selected_path->shapeB->getNode(tj)));
                }
                else
                    matchings << QString("%1|%2").arg(sid).arg(tid);
            }

            for (auto m : matchings)
            {
                auto matching = m.split("|");
                auto sid = matching.front();
                auto tid = matching.back();

                pairs << qMakePair(sid,tid);
            }
        }

        Energy::SearchNode * selected_path = &(search_roots.back());

        for (auto n : sourceModel->nodes)
        {
            n->setControlPoints(selected_path->shapeA->getNode(n->id)->controlPoints());
        }
    }

    ShapeGeometry::decodeGeometry(sourceModel);

	scene()->update(sceneBoundingRect());
}
