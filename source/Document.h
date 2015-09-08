#pragma once

#include <QObject>
#include <QVector>
#include <QSharedPointer>
#include <QVariantMap>

namespace Structure{ struct ShapeGraph; }
class Model;
namespace opengp{ namespace SurfaceMesh{ class SurfaceMeshModel; } }

class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject *parent = 0);

    // IO:
    bool loadModel(QString filename);
    void createModel(QString modelName);
    void saveModel(QString modelName, QString filename = "");
    void clearModels();

    // Datasets
    typedef QMap<QString, QVariantMap> Dataset;
    Dataset dataset;
    QString datasetPath;
    QVariantMap categories;
    QString currentCategory;
    bool loadDataset(QString datasetFolder);
    QString categoryOf(QString modelName);

	// Computed correspondence
    QMap< QString, QMap< QString, QMap<QString,QStringList> > > datasetCorr;
    void analyze(QString categoryName);
	void saveDatasetCorr(QString filename);
    void loadDatasetCorr(QString filename);

    // Compute pair-wise model matchings
    QMap< QString, QMap< QString, QVariantMap> > datasetMatching;
    void computePairwise(QString categoryName);
    void savePairwise(QString filename);
    void loadPairwise(QString filename);

    // Visualization:
    void drawModel(QString modelName, QWidget * widget);

    // Append to models:
    void createCurveFromPoints(QString modelName, QVector<QVector3D> &points);
    void createSheetFromPoints(QString modelName, QVector<QVector3D> &points);

    void duplicateActiveNodeViz(QString modelName, QString duplicationOp);
    void duplicateActiveNode(QString modelName, QString duplicationOp);

    // Modify models:
    void modifyActiveNode(QString modelName, QVector<QVector3D> &guidePoints);
    void setModelProperty(QString modelName, QString propertyName, QVariant propertyValue);
    void generateSurface(QString modelName, double offset);
    void placeOnGround(QString modelName);
    void selectPart(QString modelName, QVector3D rayOrigin, QVector3D rayDirection);
    void deselectAll(QString modelName);

    QVector3D centerActiveNode(QString modelName);
    void removeActiveNode(QString modelName);

    // Stats:
    QString firstModelName();
    QVector3D extent();

	// Direct access to models
	Model * getModel(QString name);

    // Memory access of dataset
    Model * cacheModel(QString name);

	// Helper function
	Structure::ShapeGraph * cloneAsShapeGraph(Model * m);

protected:
    QVector< QSharedPointer<Model> > models;
    QMap< QString, QSharedPointer<Model> > cachedModels;

signals:
    void analyzeProgress(int);
    void categoryAnalysisDone();
    void categoryPairwiseDone();

public slots:
    void sayCategoryAnalysisDone();
    void sayPairwiseAnalysisDone();
};

