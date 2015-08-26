#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setOrganizationName("TopoBlender");
    QApplication::setOrganizationDomain("github.com/ialhashim/TopoBlender");
    QApplication::setApplicationName("TopoBlender");

    MainWindow w;
    w.show();

    return a.exec();
}
