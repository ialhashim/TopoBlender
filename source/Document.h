#pragma once

#include <QObject>
#include <QVector>
#include <QSharedPointer>

class Model;

class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject *parent = 0);

    bool loadModel(QString filename);

    void drawModel(QString name, QWidget * widget);

    QString firstModelName();

protected:
    QVector< QSharedPointer<Model> > models;
    Model * getModel(QString name);

signals:

public slots:
};

