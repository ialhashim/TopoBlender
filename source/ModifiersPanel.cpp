#include "ModifiersPanel.h"
#include "ui_ModifiersPanel.h"

ModifiersPanel::ModifiersPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ModifiersPanel)
{
    ui->setupUi(this);
}

ModifiersPanel::~ModifiersPanel()
{
    delete ui;
}
