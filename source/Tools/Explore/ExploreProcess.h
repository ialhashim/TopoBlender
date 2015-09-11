#pragma once
#include <QColor>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>

#include <QSharedPointer>

class Document;

namespace opengp{namespace SurfaceMesh{ class SurfaceMeshModel; }}

class GraphCorresponder;
class Scheduler;
class TopoBlender;
class SynthesisManager;

#include "Thumbnail.h"

namespace ExploreProcess{
    QPair<QVector3D, QMatrix4x4> defaultCamera(double zoomFactor, int width = 128, int height = 128);
    Thumbnail::QBasicMesh toBasicMesh (opengp::SurfaceMesh::SurfaceMeshModel * m, QColor color);
    QColor qtJetColor (double v, double vmin = 0.0, double vmax = 1.0);
    Thumbnail * makeThumbnail(QGraphicsItem * parent, Document * document, QString s, QPointF pos);
    QPolygonF embed(QMap<int, QMap<int, double > > distMatrix, int embedOption);

    struct BasicVoronoiGraph{
        struct Cell{
            typedef QPair<int,int> Edge;
            std::vector<Edge> edges;
            QPointF pos;
        };
        std::vector<Cell> cells;
    };

    BasicVoronoiGraph buildGraph(QPolygonF points, double boundX, double boundY);

    struct BlendPath{
        QString source, target;
        double alpha;
        QSharedPointer<GraphCorresponder> gcorr;
        QSharedPointer<Scheduler> scheduler;
        QSharedPointer<TopoBlender> blender;
        QSharedPointer<SynthesisManager> synthManager;

        bool isReady;
        BlendPath() : isReady(false){}

        void prepare(Document * document);
        QVector<Thumbnail::QBasicMesh> blend();
    };
}
