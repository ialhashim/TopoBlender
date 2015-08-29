#pragma once
#include "Document.h"

class DocumentAnalyzeWorker : public QObject{
    Q_OBJECT
public:
    DocumentAnalyzeWorker(Document * d) : document(d){}
    Document * document;

public slots:
    void processAllPairWise();
    void processShapeDataset();

signals:
    void finished();
    void progress(int);
    void progressText(QString);
};
