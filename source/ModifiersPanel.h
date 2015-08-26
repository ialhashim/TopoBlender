#pragma once
#include <QWidget>

namespace Ui {
class ModifiersPanel;
}

class ModifiersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ModifiersPanel(QWidget *parent = 0);
    ~ModifiersPanel();

//private:
    Ui::ModifiersPanel *ui;
};
