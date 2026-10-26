#pragma once
#include <QtNodes/GraphicsView>

namespace an {
class GraphView : public QtNodes::GraphicsView {
public:
    using QtNodes::GraphicsView::GraphicsView;
protected:
    void wheelEvent(QWheelEvent *event) override;
};
}
