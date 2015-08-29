#pragma once
#include "Tool.h"

class StructureTransferView;
namespace Ui{ class StructureTransferWidget; }
class Gallery;
class Thumbnail;

class StructureTransfer : public Tool
{
    Q_OBJECT
public:
    StructureTransfer(Document * document, const QRectF &bounds);

    void init();

protected:
    StructureTransferView* view;
    Gallery* gallery;
    Ui::StructureTransferWidget* widget;
    QGraphicsProxyWidget* widgetProxy;

public slots:
    void resizeViews();
    void thumbnailSelected(Thumbnail *t);
};
