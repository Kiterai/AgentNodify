#include "graph.h"
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
    const auto oldOutputs = flow.outputCount(old), newOutputs = flow.outputCount(node);
    std::vector<int> ports(oldOutputs, -1);
    for (unsigned i = 0; i < qMin(oldOutputs, newOutputs); ++i) ports[i] = int(i);
    if (old.kind == node.kind && (node.kind == Kind::Branch || node.kind == Kind::UserInput)) {
        const unsigned oldCount = node.kind == Kind::Branch ? unsigned(old.rules.size()) : unsigned(old.userChoices.size());
        const unsigned newCount = node.kind == Kind::Branch ? unsigned(node.rules.size()) : unsigned(node.userChoices.size());
        for (unsigned i = 0; i < oldCount; ++i) ports[i] = i < newCount ? int(i) : -1;
        if (newCount < oldCount) {
            if (removedOutput && oldCount == newCount + 1 && *removedOutput < oldCount) {
                // UI removals carry their exact index: duplicate names/rules
                // must not make us disconnect a different, surviving choice.
                for (unsigned i = 0; i < oldCount; ++i) ports[i] = i == *removedOutput ? -1 : int(i > *removedOutput ? i - 1 : i);
            } else {
                // Model updates may remove multiple entries in one operation.
                // Match the surviving subsequence; fall back to index semantics
                // when entries were also renamed/edited at the same time.
                std::vector<int> matched(oldCount, -1); unsigned next = 0;
                for (unsigned i = 0; i < oldCount && next < newCount; ++i) {
                    const bool equal = node.kind == Kind::UserInput ? old.userChoices[i] == node.userChoices[next] :
                        old.rules[i].variable == node.rules[next].variable && old.rules[i].value == node.rules[next].value && old.rules[i].equals == node.rules[next].equals;
                    if (equal) matched[i] = int(next++);
                }
                if (next == newCount) for (unsigned i = 0; i < oldCount; ++i) ports[i] = matched[i];
            }
        }
        if (node.kind == Kind::Branch) ports[oldCount] = int(newCount); // Else always stays else.
    }
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
    if (node.kind == Kind::Start || node.kind == Kind::End) return nullptr;
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
    if (kind == Kind::Start || kind == Kind::End)
        for (auto &[id, n] : flow.nodes) if (n.kind == kind) return InvalidNodeId;
    Node n; n.id = newNodeId(); n.kind = kind; if (hasNodeName(kind)) n.title = kindName(kind);
    if (n.id == InvalidNodeId) return InvalidNodeId;
    n.position = newNodePosition ? newNodePosition() : QPointF(double((n.id % 4) * 220), double((n.id / 4) * 160));
    if (kind == Kind::Branch) n.rules.push_back({"result", "ok", true});
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
    case NodeRole::Size: return sizes_.contains(id) ? sizes_.at(id) : QSize(180, 80);
    case NodeRole::CaptionVisible: return n.kind != Kind::Start && n.kind != Kind::End;
    case NodeRole::Caption: {
        QString title = kindName(n.kind);
        if (states.contains(id)) {
            auto state = states.at(id);
            QString icon = state == State::Running ? QStringList{"◐", "◓", "◑", "◒"}.at(frame % 4) : state == State::Succeeded ? "✓" : state == State::Failed ? "!" : "·";
            title = icon + " " + title + " [" + stateName(state) + "]";
        }
        return title;
    }
    case NodeRole::InPortCount: return flow.inputCount(n);
    case NodeRole::OutPortCount: return flow.outputCount(n);
    case NodeRole::Widget: return QVariant::fromValue<QWidget *>(form(id));
    case NodeRole::Style: {
        NodeStyle style;
        QColor accent = n.kind == Kind::Agent ? QColor("#74b9ff") : n.kind == Kind::Script ? QColor("#bda4ff") : n.kind == Kind::Shell ? QColor("#8ecddb") : n.kind == Kind::Branch ? QColor("#ffc576") : QColor("#79d8bd");
        if (states.contains(id)) {
            if (states.at(id) == State::Running) accent = QColor("#e5bb68");
            if (states.at(id) == State::Failed) accent = QColor("#ff7c85");
            if (states.at(id) == State::Succeeded) accent = QColor("#72ddaa");
        }
        style.NormalBoundaryColor = accent; style.SelectedBoundaryColor = QColor("#ffffff");
        style.GradientColor0 = style.GradientColor1 = style.GradientColor2 = style.GradientColor3 = QColor("#252d3b");
        style.FontColor = QColor("#ecf0f7"); style.FontColorFaded = QColor("#9caec7");
        if (n.kind == Kind::UserInput) {
            style.GradientColor0 = style.GradientColor1 = style.GradientColor2 = style.GradientColor3 = QColor("#f2a4a4");
            style.FontColor = style.FontColorFaded = QColor("#40242c");
        }
        if (n.kind == Kind::Start || n.kind == Kind::End) {
            style.GradientColor0 = style.GradientColor1 = style.GradientColor2 = style.GradientColor3 = QColor("#efc65e");
            style.FontColor = style.FontColorFaded = QColor("#30260e");
            accent = QColor("#8f6d18"); style.NormalBoundaryColor = accent;
        }
        style.ConnectionPointColor = accent; style.FilledConnectionPointColor = accent;
        if (readOnly) {
            // Work selection belongs to the details pane, not graphics focus.
            // Native scene selection may disappear on a background/focus click.
            style.SelectedBoundaryColor = style.NormalBoundaryColor;
            if (id == highlightedNode_) {
                style.NormalBoundaryColor = style.SelectedBoundaryColor = QColor("#ffffff");
                style.PenWidth = style.HoveredPenWidth = 4.0f;
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
    if (role == PortRole::DataType) return QVariant::fromValue(NodeDataType{"json", "JSON / text"});
    if (role == PortRole::ConnectionPolicyRole) return QVariant::fromValue(ConnectionPolicy::Many);
    if (role == PortRole::CaptionVisible) return true;
    if (role == PortRole::Caption) {
        auto &n = flow.nodes.at(id);
        if (n.kind == Kind::Start || n.kind == Kind::End) return kindName(n.kind);
        if (type == PortType::In) return (n.kind == Kind::All || n.kind == Kind::Any) ? QString("入力 %1").arg(index) : QString("入力");
        if (n.kind == Kind::Branch) return index == n.rules.size() ? QString("それ以外") : QString("条件%1").arg(index + 1);
        if (n.kind == Kind::UserInput) return n.userChoices.value(qsizetype(index));
        return QString("出力");
    }
    return {};
}
bool Graph::deleteConnection(ConnectionId c) {
    if (!canEdit() || !connectionExists(c)) return false;
    std::erase(flow.edges, edge(c)); emit connectionDeleted(c); emit edited(); return true;
}
bool Graph::deleteNode(Id id) {
    if (!canEdit() || !nodeExists(id) || flow.nodes.at(id).kind == Kind::Start || flow.nodes.at(id).kind == Kind::End) return false;
    auto cs = allConnectionIds(id);
    std::erase_if(flow.edges, [id](const Edge &item) { return item.from == id || item.to == id; });
    for (auto c : cs) emit connectionDeleted(c);
    if (forms_.contains(id) && forms_.at(id) && !forms_.at(id)->graphicsProxyWidget()) delete forms_.at(id);
    flow.nodes.erase(id); sizes_.erase(id); forms_.erase(id); emit nodeDeleted(id); emit edited(); return true;
}
QJsonObject Graph::saveNode(Id id) const {
    Flow f; f.nodes[id] = flow.nodes.at(id);
    auto obj = QJsonDocument::fromJson(QByteArray::fromStdString(f.toJson().dump())).object();
    return {{"id", int(id)}, {"payload", obj}};
}
void Graph::loadNode(const QJsonObject &obj) {
    if (!canEdit() || !obj.contains("payload")) return;
    try {
        auto f = Flow::fromJson(Json::parse(QJsonDocument(obj["payload"].toObject()).toJson().toStdString()));
        auto n = f.nodes.begin()->second; n.id = Id(obj["id"].toInt());
        if (nodeExists(n.id)) return;
        if (n.kind == Kind::Start || n.kind == Kind::End) return;
        flow.nodes[n.id] = n; flow.nextNodeId = qMax(flow.nextNodeId, quint64(n.id) + 1); emit nodeCreated(n.id); emit edited();
    } catch (...) {}
}
}
