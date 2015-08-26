#pragma once
#include "Document.h"

class DocumentAnalyzeWorker : public QObject{
    Q_OBJECT
public:
    DocumentAnalyzeWorker(Document * d) : document(d){}
    Document * document;

public slots:
    void process();

signals:
    void finished();
    void progress(int);
};
