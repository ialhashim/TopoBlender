#pragma once
#include <QVector>
#include <QPair>
#include <QString>

namespace Structure{ struct Graph; }
class GraphCorresponder;

class ResolveCorrespondence
{
public:
    ResolveCorrespondence(Structure::Graph * shapeA,
                          Structure::Graph * shapeB,
                          QVector< QPair<QString,QString> > pairs,
                          GraphCorresponder * gcorr);
};
