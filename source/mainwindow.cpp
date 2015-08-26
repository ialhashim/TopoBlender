#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_ModifiersPanel.h"

#include <QSettings>
#include <QFileDialog>
#include <QDir>
#include <QLayout>
#include <QGraphicsProxyWidget>

#include "Document.h"
#include "Viewer.h"
#include "GraphicsScene.h"
#include "ModifiersPanel.h"

#include "Tools/Sketch/Sketch.h"
#include "Tools/ManualBlend/ManualBlend.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->show();

    QSettings settings;

    // Create document object that has shapes
    document = new Document();

    // Setup view
    auto viewport = new Viewer();
    ui->graphicsView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    ui->graphicsView->setViewport(viewport);
    ui->graphicsView->setCacheMode(QGraphicsView::CacheBackground);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    ui->graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // Create scene
    auto scene = new GraphicsScene();
    ui->graphicsView->setScene(scene);

    // Add tools window
    auto modifiers = new ModifiersPanel();
    auto modifiersWidget = scene->addWidget(modifiers);
    modifiersWidget->setObjectName("modifiersWidget");

    // Create empty model, or try load previously loaded
    QString filename = settings.value("defaultShape").toString();
    if(QFileInfo(filename).exists())
        document->loadModel(filename);
    else
        document->createModel("untitled.xml");

    // Try loading an existing dataset
    auto datasetPath = settings.value("dataset/path", "dataset").toString();
    QDir datasetDir(datasetPath);
	if (!datasetDir.exists() || QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)){
        datasetPath = QFileDialog::getExistingDirectory(this, "Open dataset", datasetPath);
        if(QDir(datasetPath).exists()){
            settings.setValue("dataset/path", datasetPath);
        }
    }

    // Load dataset details
    if(document->loadDataset( datasetPath ))
        scene->displayMessage("Dataset loaded", 1000);

    // Add tools
    {
        auto toolsRect = ui->graphicsView->rect().adjusted(0,0,-modifiers->rect().width(),0);

        // Sketch tool
        {
            auto sketcher = new Sketch(document, toolsRect);
            sketcher->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

            sketcher->setVisible(true);
            scene->addItem(sketcher);
            sketcher->moveBy(modifiers->rect().width(),0);
            sketcher->init();

            tools.push_back(sketcher);
        }

        // Manual blending
        {
            auto manualBlender = new ManualBlend(document, toolsRect);
            manualBlender->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

            manualBlender->setVisible(false);
            scene->addItem(manualBlender);
            manualBlender->moveBy(modifiers->rect().width(),0);
            manualBlender->init();

            tools.push_back(manualBlender);
        }

        // Connect buttons with tools behavior
        {
            connect(modifiers->ui->loadButton, &QPushButton::pressed, [&](){
                QSettings settings;
                QString filename = QFileDialog::getOpenFileName(0, "Load shape", "", "Shape graph (*.xml)");
                if(filename.size()){
                    document->clearModels();
                    settings.setValue("defaultShape", filename);
                    settings.sync();
                }
                document->loadModel(settings.value("defaultShape").toString());
            });

            connect(modifiers->ui->sketchButton, &QPushButton::pressed, [&](){
                showTool(Sketch::staticMetaObject.className());
            });

            connect(modifiers->ui->manualBlendButton, &QPushButton::pressed, [&](){
                document->deselectAll(document->firstModelName());
                showTool(ManualBlend::staticMetaObject.className());
            });
        }
    }
}

void MainWindow::showTool(QString className)
{
    for(auto t : tools){
        t->setVisible(false);
        if(t->metaObject()->className() == className)
            t->setVisible(true);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
