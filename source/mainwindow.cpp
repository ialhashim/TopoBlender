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
#include "Tools/AutoBlend/AutoBlend.h"
#include "Tools/StructureTransfer/StructureTransfer.h"
#include "Tools/Explore/Explore.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->show();

    QSettings settings;

    // Global options
    {
        // Background colors
        if(!settings.contains("theme")){
            settings.setValue("theme", "dark");
            settings.setValue("darkBackColor", QColor(27, 30, 32));
            settings.setValue("lightBackColor", QColor(124, 143, 162));
            settings.sync();
        }
    }

    // Create document object that has shapes
    document = new Document();

    // Setup view
    auto viewport = new Viewer();
    ui->graphicsView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    ui->graphicsView->setViewport(viewport);
    ui->graphicsView->setCacheMode(QGraphicsView::CacheBackground);
    ui->graphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    ui->graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // Application wide settings
    document->connect(ui->graphicsView, SIGNAL(globalSettingsChanged()), SLOT(sayGlobalSettingsChanged()));

    // Create scene
    auto scene = new GraphicsScene();
    ui->graphicsView->setScene(scene);

    // Add tools window
    modifiers = new ModifiersPanel();
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
        QApplication::processEvents();
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

        // Auto blending
        {
            auto autoBlender = new AutoBlend(document, toolsRect);
            autoBlender->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

            autoBlender->setVisible(false);
            scene->addItem(autoBlender);
            autoBlender->moveBy(modifiers->rect().width(),0);
            autoBlender->init();

            tools.push_back(autoBlender);
        }

        // Structure transfer
        {
            auto structureTransfer = new StructureTransfer(document, toolsRect);
            structureTransfer->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

            structureTransfer->setVisible(false);
            scene->addItem(structureTransfer);
            structureTransfer->moveBy(modifiers->rect().width(),0);
            structureTransfer->init();

            tools.push_back(structureTransfer);
        }

        // Explore
        {
            auto explore = new Explore(document, toolsRect);
            explore->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

            explore->setVisible(false);
            scene->addItem(explore);
            explore->moveBy(modifiers->rect().width(),0);
            explore->init();

            tools.push_back(explore);
        }

        // Connect buttons with tools behavior
        {
            connect(modifiers->ui->loadButton, &QPushButton::pressed, [&](){
                document->clearModels();
                QString filename = QFileDialog::getOpenFileName(0, "Load shape", "", "Shape graph (*.xml)");
                if(filename.trimmed().size()){
                    document->loadModel(filename);
                    QSettings().setValue("defaultShape",filename);
                }
                else
                    document->createModel("untitled");
            });

            connect(modifiers->ui->sketchButton, &QPushButton::pressed, [&](){
                showTool(Sketch::staticMetaObject.className());
            });

            connect(modifiers->ui->manualBlendButton, &QPushButton::pressed, [&](){
                document->deselectAll(document->firstModelName());
                showTool(ManualBlend::staticMetaObject.className());
            });

            connect(modifiers->ui->autoBlendButton, &QPushButton::pressed, [&](){
                document->deselectAll(document->firstModelName());
                showTool(AutoBlend::staticMetaObject.className());
            });

            connect(modifiers->ui->structureTransferButton, &QPushButton::pressed, [&](){
                document->deselectAll(document->firstModelName());
                showTool(StructureTransfer::staticMetaObject.className());
            });

            connect(modifiers->ui->exploreButton, &QPushButton::pressed, [&](){
                document->deselectAll(document->firstModelName());
                showTool(Explore::staticMetaObject.className());
            });
        }
    }

    QApplication::processEvents();
    scene->update();
}

void MainWindow::showTool(QString className)
{
    for(auto t : tools){
        t->setVisible(false);
        if(t->metaObject()->className() == className)
            t->setVisible(true);
    }

    modifiers->update();

    // Qt bug?
    {
        modifiers->ui->sketchButton->setAttribute(Qt::WA_UnderMouse, false);
        modifiers->ui->manualBlendButton->setAttribute(Qt::WA_UnderMouse, false);
        modifiers->ui->autoBlendButton->setAttribute(Qt::WA_UnderMouse, false);
        modifiers->ui->structureTransferButton->setAttribute(Qt::WA_UnderMouse, false);
        modifiers->ui->exploreButton->setAttribute(Qt::WA_UnderMouse, false);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
