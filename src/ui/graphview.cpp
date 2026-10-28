#include "shared/constants.h"
#include "graphview.h"
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneWheelEvent>
#include <QCoreApplication>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtNodes/internal/AbstractNodeGeometry.hpp>
#include <QtNodes/internal/NodeGraphicsObject.hpp>
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QtNodes/BasicGraphicsScene>
#include <cmath>

namespace an {
void GraphView::mousePressEvent(QMouseEvent *event) {
    if (interactionStarted) interactionStarted();
    trackingInteraction_ = false; interactionNode_.reset();
    if (event->button() == Qt::LeftButton && beforePortEdit) {
        auto *graphScene = dynamic_cast<QtNodes::BasicGraphicsScene *>(scene());
        for (auto *item = itemAt(event->position().toPoint()); graphScene && item; item = item->parentItem()) {
            if (auto *node = dynamic_cast<QtNodes::NodeGraphicsObject *>(item)) {
                const auto point = node->mapFromScene(mapToScene(event->position().toPoint()));
                for (auto type : {QtNodes::PortType::In, QtNodes::PortType::Out})
                    if (graphScene->nodeGeometry().checkPortHit(node->nodeId(), type, point) != QtNodes::InvalidPortIndex && !beforePortEdit()) { event->accept(); return; }
                break;
            }
        }
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        auto *hit = itemAt(event->position().toPoint());
        trackingInteraction_ = !hit;
        for (auto *item = hit; item; item = item->parentItem()) {
            if (auto *node = dynamic_cast<QtNodes::NodeGraphicsObject *>(item)) {
                interactionNode_ = node->nodeId(); trackingInteraction_ = true; break;
            }
        }
    }
    // Popups can take the release event away from this view. Remember the
    // click immediately, then update its final scene location after a drag.
    if (trackingInteraction_ && interactionFinished) interactionFinished(interactionNode_, mapToScene(event->position().toPoint()));
    QtNodes::GraphicsView::mousePressEvent(event);
}
void GraphView::mouseReleaseEvent(QMouseEvent *event) {
    QtNodes::GraphicsView::mouseReleaseEvent(event);
    if (trackingInteraction_ && interactionFinished && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton || event->button() == Qt::RightButton))
        interactionFinished(interactionNode_, mapToScene(event->position().toPoint()));
    trackingInteraction_ = false; interactionNode_.reset();
}
void GraphView::zoomToNode(QtNodes::NodeId id) {
    auto *graphScene = dynamic_cast<QtNodes::BasicGraphicsScene *>(scene());
    auto *node = graphScene ? graphScene->nodeGraphicsObject(id) : nullptr;
    if (!node) return;
    const auto bounds = node->sceneBoundingRect(); constexpr qreal margin = metrics::NodeZoomMargin;
    if (bounds.isEmpty() || viewport()->width() <= 2 * margin || viewport()->height() <= 2 * margin) return;
    const auto scale = qMin(metrics::NodeZoomMaximum, qMin((viewport()->width() - 2 * margin) / bounds.width(), (viewport()->height() - 2 * margin) / bounds.height()));
    if (!std::isfinite(scale) || scale <= 0) return;
    const QSizeF visible(viewport()->width() / scale, viewport()->height() / scale);
    const QRectF centered(bounds.center() - QPointF(visible.width() / 2, visible.height() / 2), visible);
    setSceneRect(sceneRect().united(centered.adjusted(-margin / scale, -margin / scale, margin / scale, margin / scale)));
    setTransform(QTransform::fromScale(scale, scale)); centerOn(bounds.center()); emit scaleChanged(scale);
}
void GraphView::zoomToFit() {
    if (!scene()) return;
    QRectF bounds;
    for (auto *item : scene()->items()) {
        if (!item->isVisible()) continue;
        if (dynamic_cast<QtNodes::NodeGraphicsObject *>(item) || dynamic_cast<QtNodes::ConnectionGraphicsObject *>(item))
            bounds = bounds.united(item->sceneBoundingRect());
    }
    constexpr qreal margin = metrics::FitZoomMargin;
    if (bounds.isEmpty() || viewport()->width() <= 2 * margin || viewport()->height() <= 2 * margin) return;
    const auto scale = qMin((viewport()->width() - 2 * margin) / bounds.width(), (viewport()->height() - 2 * margin) / bounds.height());
    if (!std::isfinite(scale) || scale <= 0) return;
    // Reset any panned view rect, including when nodes are outside QtNodes' initial
    // scene range. Fitting may need a scale smaller than the manual wheel minimum.
    const auto padding = margin / scale;
    setSceneRect(bounds.adjusted(-padding, -padding, padding, padding));
    setTransform(QTransform::fromScale(scale, scale)); centerOn(bounds.center());
    emit scaleChanged(scale);
}
void GraphView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier && hasFocus() && scene() && !scene()->focusItem()) {
        if (!event->isAutoRepeat()) zoomToFit();
        event->accept(); return;
    }
    QtNodes::GraphicsView::keyPressEvent(event);
}
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
