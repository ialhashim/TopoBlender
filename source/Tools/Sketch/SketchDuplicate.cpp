#include "SketchDuplicate.h"
#include "ui_SketchDuplicate.h"

#include <QMouseEvent>
#include <QKeyEvent>

#include <QMessageBox>
#include <QDebug>

SketchDuplicate::SketchDuplicate(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SketchDuplicate)
{
    ui->setupUi(this);

    connect(ui->acceptButton, SIGNAL(pressed()), SLOT(acceptOperation()));

    connect(ui->dupT, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->dupRef, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->dupRot, SIGNAL(toggled(bool)), SLOT(changeSettings()));

    connect(ui->tCount, SIGNAL(valueChanged(int)), SLOT(changeSettings()));
    connect(ui->tX, SIGNAL(valueChanged(double)), SLOT(changeSettings()));
    connect(ui->tY, SIGNAL(valueChanged(double)), SLOT(changeSettings()));
    connect(ui->tZ, SIGNAL(valueChanged(double)), SLOT(changeSettings()));

    connect(ui->refX, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->refY, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->refZ, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->refOffset, SIGNAL(valueChanged(double)), SLOT(changeSettings()));

    connect(ui->rot_Count, SIGNAL(valueChanged(int)), SLOT(changeSettings()));
    connect(ui->rotX, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->rotY, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->rotZ, SIGNAL(toggled(bool)), SLOT(changeSettings()));
    connect(ui->rotOffset, SIGNAL(valueChanged(double)), SLOT(changeSettings()));
}

SketchDuplicate::~SketchDuplicate()
{
    delete ui;
}

QString SketchDuplicate::dupOperation()
{
    QString opDupT, opDupRef, opDupRot;

    opDupT = QString("dupT,%1,%2,%3,%4")
            .arg(ui->tCount->value())
            .arg(ui->tX->value())
            .arg(ui->tY->value())
            .arg(ui->tZ->value());

    opDupRef = QString("dupRef,%1,%2,%3,%4")
            .arg(ui->refX->isChecked())
            .arg(ui->refY->isChecked())
            .arg(ui->refZ->isChecked())
            .arg(ui->refOffset->value());

    opDupRot = QString("dupRot,%1,%2,%3,%4,%5")
            .arg(ui->rot_Count->value())
            .arg(ui->rotX->isChecked())
            .arg(ui->rotY->isChecked())
            .arg(ui->rotZ->isChecked())
            .arg(ui->rotOffset->value());

    QString op;
    if(ui->dupT->isChecked()) op = opDupT;
    if(ui->dupRef->isChecked()) op = opDupRef;
    if(ui->dupRot->isChecked()) op = opDupRot;
    return op;
}

void SketchDuplicate::acceptOperation()
{
    emit(acceptedOperation());
}

void SketchDuplicate::changeSettings()
{
    emit(settingsChanged());
}
