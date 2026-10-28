#include "constants.h"
#include "strings.h"
#include "nodedefinition.h"
#include "graph.h"
#include "theme.h"
#include <QtNodes/NodeStyle>
#include <QJsonDocument>
#include <QWidget>
#include <algorithm>

namespace an {
using namespace QtNodes;
namespace {
ConnectionId connection(const Edge &e) { return {e.from, e.output, e.to, e.input}; }
Edge edge(ConnectionId c) { return {c.outNodeId, c.inNodeId, c.outPortIndex, c.inPortIndex}; }
}
Graph::~Graph() {
    for (auto &[id, widget] : forms_) if (widget && !widget->graphicsProxyWidget()) delete widget;
}
bool Graph::canEdit() const {
    if (readOnly) return false;
    if (editsLocked && editsLocked()) { emit const_cast<Graph *>(this)->editRejected(); return false; }
    return true;
}
void Graph::replace(const Flow &value) {
    for (auto &[id, widget] : forms_) if (widget && !widget->graphicsProxyWidget()) delete widget;
    flow = value;
    for (auto &[id, node] : flow.nodes) if (!hasNodeName(node.kind)) node.title.clear();
    if (!nodeExists(highlightedNode_)) highlightedNode_ = 0;
    if (!flow.nodes.empty()) flow.nextNodeId = qMax(flow.nextNodeId, quint64(flow.nodes.rbegin()->first) + 1);
    sizes_.clear(); states.clear(); forms_.clear(); emit modelReset();
}
void Graph::restore(const Flow &value) {
    const auto previous = flow;
    auto target = value; target.nextNodeId = qMax(flow.nextNodeId, target.nextNodeId);
    for (auto &[id, node] : target.nodes) if (!hasNodeName(node.kind)) node.title.clear();
    flow = std::move(target);
    for (const auto &item : previous.edges)
        if (std::find(flow.edges.begin(), flow.edges.end(), item) == flow.edges.end()) emit connectionDeleted(connection(item));
    for (const auto &[id, node] : previous.nodes) if (!nodeExists(id)) {
        if (forms_.contains(id) && forms_.at(id) && !forms_.at(id)->graphicsProxyWidget()) delete forms_.at(id);
        forms_.erase(id); sizes_.erase(id); states.erase(id); emit nodeDeleted(id);
    }
    for (const auto &[id, node] : flow.nodes) {
        if (!previous.nodes.contains(id)) emit nodeCreated(id);
        else {
            if (forms_.contains(id) && forms_.at(id)) forms_.at(id)->setNode(node);
            emit nodeUpdated(id);
            if (previous.nodes.at(id).position != node.position) emit nodePositionUpdated(id);
        }
    }
    for (const auto &item : flow.edges)
        if (std::find(previous.edges.begin(), previous.edges.end(), item) == previous.edges.end()) emit connectionCreated(connection(item));
    if (!nodeExists(highlightedNode_)) highlightedNode_ = 0;
    emit edited();
}
void Graph::setHighlightedNode(Id id) {
    if (!nodeExists(id)) id = 0;
    if (highlightedNode_ == id) return;
    const auto previous = highlightedNode_; highlightedNode_ = id;
    if (nodeExists(previous)) emit nodeUpdated(previous);
    if (nodeExists(id)) emit nodeUpdated(id);
}
void Graph::updateNode(const Node &node, std::optional<unsigned> removedOutput) {
    if (!nodeExists(node.id)) return;
    if (!canEdit()) {
        if (nodeExists(node.id) && forms_.contains(node.id) && forms_.at(node.id)) forms_.at(node.id)->setNode(flow.nodes.at(node.id));
        return;
    }
    const auto &old = flow.nodes.at(node.id);
    auto ports = remapNodeOutputs(old, node, removedOutput);
    std::vector<Edge> kept; std::vector<ConnectionId> removed, moved;
    for (const auto &edge : flow.edges) {
        auto updated = edge; bool drop = edge.to == node.id && edge.input >= flow.inputCount(node);
        if (edge.from == node.id) {
            if (edge.output >= ports.size() || ports[edge.output] < 0) drop = true;
            else updated.output = unsigned(ports[edge.output]);
        }
        if (drop) removed.push_back(connection(edge));
        else {
            kept.push_back(updated);
            if (!(updated == edge)) { removed.push_back(connection(edge)); moved.push_back(connection(updated)); }
        }
    }
    // Publish a consistent model before notifying QtNodes. Keep the original
    // edge order for FIFO fan-out, and rebuild only removed/reindexed graphics.
    auto updated = node; updated.position = old.position;
    if (!hasNodeName(updated.kind)) updated.title.clear();
    flow.nodes[node.id] = updated; flow.edges = std::move(kept);
    for (const auto &port : removed) emit connectionDeleted(port);
    if (forms_.contains(node.id) && forms_.at(node.id)) forms_.at(node.id)->setNode(updated);
    emit nodeUpdated(node.id);
    for (const auto &port : moved) emit connectionCreated(port);
    emit edited();
}
void Graph::setFlowChoices(const std::vector<std::pair<QString, QString>> &choices) {
    flowChoices = choices;
    for (auto &[id, form] : forms_) if (form) form->setFlowChoices(choices);
}
NodeForm *Graph::form(Id id) const {
    if (!nodeExists(id)) return nullptr;
    const auto &node = flow.nodes.at(id);
    if (nodeDefinition(node.kind).source || nodeDefinition(node.kind).terminal) return nullptr;
    if (forms_[id]) return forms_[id];
    auto *self = const_cast<Graph *>(this);
    auto *widget = new NodeForm(node, nullptr, modelCatalog); forms_[id] = widget;
    widget->setReadOnly(readOnly);
    widget->beforeEdit = [self] { return self->canEdit(); };
    widget->setFlowChoices(flowChoices);
    connect(widget, &NodeForm::edited, self, [self, widget] { self->updateNode(widget->node()); });
    connect(widget, &NodeForm::outputRemoved, self, [self, widget](unsigned index) { self->updateNode(widget->node(), index); });
    connect(widget, &NodeForm::activated, self, [self, id] { emit self->nodeActivated(id); });
    connect(widget, &NodeForm::textEditRequested, self, [self, id](const QString &field) { emit self->textEditRequested(id, field); });
    connect(widget, &NodeForm::sizeChanged, self, [self, id] { if (self->nodeExists(id)) emit self->nodeUpdated(id); });
    return widget;
}
Id Graph::newNodeId() {
    if (!flow.nodes.empty()) flow.nextNodeId = qMax(flow.nextNodeId, quint64(flow.nodes.rbegin()->first) + 1);
    if (flow.nextNodeId >= InvalidNodeId) return InvalidNodeId;
    return Id(flow.nextNodeId++);
}
std::unordered_set<Id> Graph::allNodeIds() const {
    std::unordered_set<Id> result; for (auto &[id, n] : flow.nodes) result.insert(id); return result;
}
std::unordered_set<ConnectionId> Graph::allConnectionIds(Id id) const {
    std::unordered_set<ConnectionId> result;
    for (auto &e : flow.edges) if (e.from == id || e.to == id) result.insert(connection(e));
    return result;
}
std::unordered_set<ConnectionId> Graph::connections(Id id, PortType type, unsigned index) const {
    std::unordered_set<ConnectionId> result;
    for (auto &e : flow.edges) if ((type == PortType::In && e.to == id && e.input == index) || (type == PortType::Out && e.from == id && e.output == index)) result.insert(connection(e));
    return result;
}
bool Graph::connectionExists(ConnectionId c) const { return std::find(flow.edges.begin(), flow.edges.end(), edge(c)) != flow.edges.end(); }
Id Graph::addNode(QString type) {
    if (!canEdit()) return InvalidNodeId;
    Kind kind;
    try { kind = parseKind(type.toStdString()); } catch (...) { return InvalidNodeId; }
    if (nodeDefinition(kind).source || nodeDefinition(kind).terminal)
        for (auto &[id, n] : flow.nodes) if (n.kind == kind) return InvalidNodeId;
    Node n; n.id = newNodeId(); n.kind = kind; if (hasNodeName(kind)) n.title = kindName(kind);
    if (n.id == InvalidNodeId) return InvalidNodeId;
    n.position = newNodePosition ? newNodePosition() : QPointF(double((n.id % metrics::NodeColumns) * metrics::NodeColumnStep), double((n.id / metrics::NodeColumns) * metrics::NodeRowStep));
    nodeDefinition(kind).initialize(n);
    flow.nodes[n.id] = n; emit nodeCreated(n.id); emit edited(); return n.id;
}
bool Graph::connectionPossible(ConnectionId c) const {
    if (readOnly || (editsLocked && editsLocked()) || !nodeExists(c.outNodeId) || !nodeExists(c.inNodeId) || connectionExists(c)) return false;
    if (c.inPortIndex >= flow.inputCount(flow.nodes.at(c.inNodeId)) || c.outPortIndex >= flow.outputCount(flow.nodes.at(c.outNodeId))) return false;
    return true;
}
void Graph::addConnection(ConnectionId c) {
    if (!canEdit()) return;
    if (!connectionPossible(c)) return;
    flow.edges.push_back(edge(c)); emit connectionCreated(c); emit edited();
}
QVariant Graph::nodeData(Id id, NodeRole role) const {
    const auto &n = flow.nodes.at(id);
    switch(role) {
    case NodeRole::Type: return kindKey(n.kind);
    case NodeRole::Position: return n.position;
    case NodeRole::Size: return sizes_.contains(id) ? sizes_.at(id) : QSize(metrics::FallbackNodeWidth, metrics::FallbackNodeHeight);
    case NodeRole::CaptionVisible: return !nodeDefinition(n.kind).source && !nodeDefinition(n.kind).terminal;
    case NodeRole::Caption: {
        QString title = kindName(n.kind);
        if (states.contains(id)) {
            auto state = states.at(id);
            QString icon = state == State::Running ? QStringList{strings::RunningFrameFirst, strings::RunningFrameSecond, strings::RunningFrameThird, strings::RunningFrameFourth}.at(frame % metrics::RunningAnimationFrames) : state == State::Succeeded ? strings::SuccessSymbol : state == State::Failed ? strings::WarningSymbol : strings::IdleSymbol;
            title = icon + strings::Space + title + strings::StateOpeningBracket + stateName(state) + strings::ClosingBracket;
        }
        return title;
    }
    case NodeRole::InPortCount: return flow.inputCount(n);
    case NodeRole::OutPortCount: return flow.outputCount(n);
    case NodeRole::Widget: return QVariant::fromValue<QWidget *>(form(id));
    case NodeRole::Style: {
        NodeStyle style;
        QColor accent = nodeAccent(n.kind);
        if (states.contains(id)) {
            if (states.at(id) == State::Running) accent = QColor(colors::RunningBoundary);
            if (states.at(id) == State::Failed) accent = QColor(colors::FailedBoundary);
            if (states.at(id) == State::Succeeded) accent = QColor(colors::SucceededBoundary);
        }
        style.NormalBoundaryColor = accent; style.SelectedBoundaryColor = QColor(colors::White);
        style.GradientColor0 = style.GradientColor1 = style.GradientColor2 = style.GradientColor3 = nodeBackground(n.kind);
        style.FontColor = nodeForeground(n.kind); style.FontColorFaded = hasSpecialNodeBackground(n.kind) ? nodeForeground(n.kind) : QColor(colors::MutedText);
        if (nodeDefinition(n.kind).source || nodeDefinition(n.kind).terminal) {
            accent = nodeAccent(n.kind); style.NormalBoundaryColor = accent;
        }
        style.ConnectionPointColor = accent; style.FilledConnectionPointColor = accent;
        if (readOnly) {
            // Work selection belongs to the details pane, not graphics focus.
            // Native scene selection may disappear on a background/focus click.
            style.SelectedBoundaryColor = style.NormalBoundaryColor;
            if (id == highlightedNode_) {
                style.NormalBoundaryColor = style.SelectedBoundaryColor = QColor(colors::White);
                style.PenWidth = style.HoveredPenWidth = metrics::HighlightBoundaryWidth;
            }
        }
        return style.toJson().toVariantMap();
    }
    default: return {};
    }
}
NodeFlags Graph::nodeFlags(Id) const { return NodeFlag::NoFlags; }
bool Graph::setNodeData(Id id, NodeRole role, QVariant value) {
    if (!nodeExists(id)) return false;
    if (role == NodeRole::Size) { sizes_[id] = value.toSize(); return true; }
    if (role == NodeRole::Position) {
        flow.nodes[id].position = value.toPointF(); emit nodePositionUpdated(id); emit edited(); return true;
    }
    return false;
}
QVariant Graph::portData(Id id, PortType type, unsigned index, PortRole role) const {
    if (role == PortRole::DataType) return QVariant::fromValue(NodeDataType{strings::JsonFormat, strings::JSONText});
    if (role == PortRole::ConnectionPolicyRole) return QVariant::fromValue(ConnectionPolicy::Many);
    if (role == PortRole::CaptionVisible) return true;
    if (role == PortRole::Caption) {
        auto &n = flow.nodes.at(id);
        return nodeDefinition(n.kind).portCaption(n, type == PortType::In, index);
    }
    return {};
}
bool Graph::deleteConnection(ConnectionId c) {
    if (!canEdit() || !connectionExists(c)) return false;
    std::erase(flow.edges, edge(c)); emit connectionDeleted(c); emit edited(); return true;
}
bool Graph::deleteNode(Id id) {
    if (!canEdit() || !nodeExists(id) || nodeDefinition(flow.nodes.at(id).kind).source) return false;
    auto cs = allConnectionIds(id);
    std::erase_if(flow.edges, [id](const Edge &item) { return item.from == id || item.to == id; });
    for (auto c : cs) emit connectionDeleted(c);
    if (forms_.contains(id) && forms_.at(id) && !forms_.at(id)->graphicsProxyWidget()) delete forms_.at(id);
    flow.nodes.erase(id); sizes_.erase(id); forms_.erase(id); emit nodeDeleted(id); emit edited(); return true;
}
QJsonObject Graph::saveNode(Id id) const {
    Flow f; f.nodes[id] = flow.nodes.at(id);
    auto obj = QJsonDocument::fromJson(QByteArray::fromStdString(f.toJson().dump())).object();
    return {{strings::Id, int(id)}, {strings::Payload, obj}};
}
void Graph::loadNode(const QJsonObject &obj) {
    if (!canEdit() || !obj.contains(strings::Payload)) return;
    try {
        auto f = Flow::fromJson(Json::parse(QJsonDocument(obj[strings::Payload].toObject()).toJson().toStdString()));
        auto n = f.nodes.begin()->second; n.id = Id(obj[strings::Id].toInt());
        if (nodeExists(n.id)) return;
        if (nodeDefinition(n.kind).source) return;
        if (nodeDefinition(n.kind).terminal) for(const auto &[id,node]:flow.nodes) if(nodeDefinition(node.kind).terminal)return;
        flow.nodes[n.id] = n; flow.nextNodeId = qMax(flow.nextNodeId, quint64(n.id) + 1); emit nodeCreated(n.id); emit edited();
    } catch (...) {}
}
}
