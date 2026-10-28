#pragma once
#include <QtNodes/GraphicsView>
#include <QtNodes/AbstractGraphModel>
#include <functional>
#include <optional>

namespace an {
class GraphView : public QtNodes::GraphicsView {
public:
    using QtNodes::GraphicsView::GraphicsView;
    void zoomToFit();
    void zoomToNode(QtNodes::NodeId id);
    std::function<bool()> beforePortEdit;
    std::function<void(std::optional<QtNodes::NodeId>, QPointF)> interactionFinished;
    std::function<void()> interactionStarted;
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    bool trackingInteraction_ = false;
    std::optional<QtNodes::NodeId> interactionNode_;
};
}
