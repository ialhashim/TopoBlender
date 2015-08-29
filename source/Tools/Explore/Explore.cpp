#include "Explore.h"
#include "ui_Explore.h"

#include "Document.h"
#include "Camera.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"

#include "Thumbnail.h"

#include "SimpleMatrix.h"

#include "Model.h"

Explore::Explore(Document *document, const QRectF &bounds) : Tool(document)
{
    setBounds(bounds);
    setObjectName("explore");
}

void Explore::init()
{
    // Add widget
    auto toolsWidgetContainer = new QWidget();
    widget = new Ui::ExploreWidget();
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

    // Connect UI with actions
    {
        connect(widget->categoriesBox, &QComboBox::currentTextChanged, [&](QString text){
            document->currentCategory = text;
        });

        connect(widget->analyzeButton, &QPushButton::pressed, [&](){
            document->computePairwise(widget->categoriesBox->currentText());
        });

        connect(document, &Document::categoryPairwiseDone, [=](){
            auto catModels = document->categories[ document->currentCategory ].toStringList();

            smat::Matrix<double> D(catModels.size(), catModels.size(), 0);

            for(auto s : document->datasetMatching.keys())
            {
                for(auto t : document->datasetMatching[s].keys())
                {
                    int i = catModels.indexOf(s);
                    int j = catModels.indexOf(t);

                    double dist = document->datasetMatching[s][t]["min_cost"].toDouble();

                    D.set(i,j,dist);
                    D.set(j,i,dist);
                }
            }

            // Default view angle
            {
                // Camera target and initial position
                auto camera = new Eigen::Camera();
                auto frame = camera->frame();
                frame.position = Eigen::Vector3f(-1, 0, 0.5);
                camera->setTarget(Eigen::Vector3f(0, 0, 0.5));
                camera->setFrame(frame);

                double theta1 = acos(-1) * 0.75;
                double theta2 = acos(-1) * 0.10;
                camera->rotateAroundTarget(Eigen::Quaternionf(Eigen::AngleAxisf(theta1, Eigen::Vector3f::UnitY())));
                camera->zoom(-4);
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

            // MDS
            auto X = QSharedPointer< smat::Matrix<double> >(smat::MDS_SMACOF(&D, NULL, 2, 100));

            /// Plot:

            // Compute range:
            QPolygonF allPoints;
            for(auto s : document->datasetMatching.keys()){
                int i = catModels.indexOf(s);
                allPoints << QPointF(X->get(i,0), X->get(i,1));
            }
            auto allPointsRect = allPoints.boundingRect();

            // Plot points
            double dx = bounds.width() * 0.2;
            double dy = bounds.height() * 0.2;
            QRectF r(0, 0, bounds.width() - dx, bounds.height() - dy);

            QRectF thumbRect(0,0,92,92);

            for(auto s : document->datasetMatching.keys())
            {
                int i = catModels.indexOf(s);
                auto posRelative = QPointF((X->get(i,0) - allPointsRect.left())/allPointsRect.width(),
                                            (X->get(i,1) - allPointsRect.top())/allPointsRect.height());
                auto pos = QPointF((posRelative.x() * r.width()) + (dx/2.0), (posRelative.y() * r.height()) + (dy/2.0));

                auto t = new Thumbnail(this, thumbRect);
                t->setCaption(s);
                t->setMesh();
                t->setCamera(cameraPos, cameraMatrix);
                t->setFlag(QGraphicsItem::ItemIsSelectable);

                // Add parts of target shape
                auto m = document->cacheModel(s);
                for (auto n : m->nodes){
                    t->addAuxMesh(toBasicMesh(m->getMesh(n->id), n->vis_property["color"].value<QColor>()));
                }

                t->setPos(pos - thumbRect.center());
            }
        });
    }
}
