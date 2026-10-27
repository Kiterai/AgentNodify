#pragma once
#include <QtNodes/GraphicsView>

namespace an {
class GraphView : public QtNodes::GraphicsView {
public:
    using QtNodes::GraphicsView::GraphicsView;
    void zoomToFit();
protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};
}
