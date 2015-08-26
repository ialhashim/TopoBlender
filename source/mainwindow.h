#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class Document;
class Tool;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void showTool(QString className);

protected:
    QVector<Tool*> tools;
    Document * document;

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
