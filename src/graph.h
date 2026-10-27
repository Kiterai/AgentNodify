#pragma once
#include "flow.h"
#include "runner.h"
#include <QtNodes/AbstractGraphModel>
#include <QtNodes/NodeData>
#include <QSize>
#include <QPointer>
#include "nodeform.h"

namespace an {
class Graph : public QtNodes::AbstractGraphModel {
    Q_OBJECT
public:
    ~Graph() override;
    Flow flow;
    std::vector<std::pair<QString, QString>> flowChoices;
    void setFlowChoices(const std::vector<std::pair<QString, QString>> &choices);
    bool readOnly = false;
    QPointer<ModelCatalog> modelCatalog;
    std::map<Id, State> states;
    int frame = 0;
    void replace(const Flow &value);
    void setHighlightedNode(Id id);
    void updateNode(const Node &node);
    NodeForm *form(Id id) const;
    QtNodes::NodeId newNodeId() override;
    std::unordered_set<Id> allNodeIds() const override;
    std::unordered_set<QtNodes::ConnectionId> allConnectionIds(Id id) const override;
    std::unordered_set<QtNodes::ConnectionId> connections(Id id, QtNodes::PortType type, unsigned index) const override;
    bool connectionExists(QtNodes::ConnectionId id) const override;
    Id addNode(QString type = {}) override;
    bool connectionPossible(QtNodes::ConnectionId id) const override;
    bool detachPossible(QtNodes::ConnectionId) const override { return !readOnly; }
    void addConnection(QtNodes::ConnectionId id) override;
    bool nodeExists(Id id) const override { return flow.nodes.contains(id); }
    QVariant nodeData(Id id, QtNodes::NodeRole role) const override;
    QtNodes::NodeFlags nodeFlags(Id) const override;
    bool setNodeData(Id id, QtNodes::NodeRole role, QVariant value) override;
    QVariant portData(Id id, QtNodes::PortType type, unsigned index, QtNodes::PortRole role) const override;
    bool setPortData(Id, QtNodes::PortType, unsigned, const QVariant &, QtNodes::PortRole) override { return false; }
    bool deleteConnection(QtNodes::ConnectionId id) override;
    bool deleteNode(Id id) override;
    QJsonObject saveNode(Id id) const override;
    void loadNode(const QJsonObject &json) override;
signals:
    void edited();
    void textEditRequested(unsigned int id, QString field);
    void nodeActivated(unsigned int id);
private:
    Id highlightedNode_ = 0;
    std::map<Id, QSize> sizes_;
    mutable std::map<Id, QPointer<NodeForm>> forms_;
};
}
