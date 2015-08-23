#ifndef SKETCHDUPLICATE_H
#define SKETCHDUPLICATE_H

#include <QWidget>

namespace Ui {
class SketchDuplicate;
}

class SketchDuplicate : public QWidget
{
    Q_OBJECT

public:
    explicit SketchDuplicate(QWidget *parent = 0);
    ~SketchDuplicate();

//private:
    Ui::SketchDuplicate *ui;

    QString dupOperation();

public slots:
    void acceptOperation();
    void changeSettings();

signals:
    void settingsChanged();
    void acceptedOperation();
};

#endif // SKETCHDUPLICATE_H
