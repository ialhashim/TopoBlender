#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QLayout>
#include <QGraphicsProxyWidget>

#include "Viewer.h"
#include "GraphicsScene.h"
#include "ModifiersPanel.h"

#include "Document.h"

#include "Tools/Sketch/Sketch.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->show();

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

    // Create document object that has shapes
    auto document = new Document();
    document->loadModel("C:/Temp/ChairBasic1/SimpleChair1.xml");

    // Add tools window
    auto modifiers = new ModifiersPanel();
    auto modifiersWidget = scene->addWidget(modifiers);
    modifiersWidget->setObjectName("modifiersWidget");

    // Add tools
    auto sketcher = new Sketch(document, ui->graphicsView->rect().adjusted(0,0,-modifiers->rect().width(),0) );
    sketcher->connect(ui->graphicsView, SIGNAL(resized(QRectF)), SLOT(setBounds(QRectF)));

    scene->addItem(sketcher);
    sketcher->moveBy(modifiers->rect().width(),0);
    sketcher->init();
}

MainWindow::~MainWindow()
{
    delete ui;
}
