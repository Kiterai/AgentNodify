#pragma once
#include <QtNodes/GraphicsView>
#include <QtNodes/AbstractGraphModel>

namespace an {
class GraphView : public QtNodes::GraphicsView {
public:
    using QtNodes::GraphicsView::GraphicsView;
    void zoomToFit();
    void zoomToNode(QtNodes::NodeId id);
protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};
}
