#pragma once

#include <QObject>
#include <QSharedPointer>

#include <Eigen/Core>

class Document;
class GraphCorresponder;
class TopoBlender;
class Scheduler;
class SynthesisManager;

class ManualBlendManager : public QObject
{
    Q_OBJECT
public:
    ManualBlendManager(Document * document);

    Document * document;

    QString sourcePartName, targetName, targetPartName;

	QSharedPointer<GraphCorresponder> gcorr;
	QSharedPointer<Scheduler> scheduler;
    QSharedPointer<TopoBlender> blender;
    QSharedPointer<SynthesisManager> synthManager;

	bool isBusy;

public slots:
    void doBlend(QString sourcePartName, QString targetName, QString targetPartName, int value);
	void finalizeBlend(QString sourcePartName, QString targetName, QString targetPartName, int value);
	
signals:
	void cloudReady(QPair< QVector<Eigen::Vector3f>, QVector<Eigen::Vector3f> >);
};
