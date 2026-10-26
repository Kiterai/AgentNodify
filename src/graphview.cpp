#include "graphview.h"
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneWheelEvent>
#include <QCoreApplication>
#include <QWheelEvent>

namespace an {
void GraphView::wheelEvent(QWheelEvent *event) {
    // QtNodes zooms without first delivering wheel events to embedded widgets.
    // This includes combo popups, which are themselves child proxy widgets.
    for (auto *item = itemAt(event->position().toPoint()); item; item = item->parentItem()) {
        if (qgraphicsitem_cast<QGraphicsProxyWidget *>(item)) {
            // Deliver directly to the scene. QGraphicsView's fallback scrolls
            // the canvas when a list ignores a wheel event at its boundary.
            QGraphicsSceneWheelEvent wheel(QEvent::GraphicsSceneWheel);
            wheel.setWidget(viewport()); wheel.setScenePos(mapToScene(event->position().toPoint()));
            wheel.setScreenPos(event->globalPosition().toPoint());
            wheel.setButtons(event->buttons()); wheel.setModifiers(event->modifiers());
            const bool vertical = event->angleDelta().y() != 0 || event->angleDelta().x() == 0;
            wheel.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
            wheel.setDelta(vertical ? event->angleDelta().y() : event->angleDelta().x());
            wheel.setPixelDelta(event->pixelDelta()); wheel.setPhase(event->phase()); wheel.setInverted(event->inverted());
            QCoreApplication::sendEvent(scene(), &wheel);
            event->accept(); // Never zoom or pan the canvas over a form.
            return;
        }
    }
    QtNodes::GraphicsView::wheelEvent(event);
}
}
