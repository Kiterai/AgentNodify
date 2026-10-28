#pragma once
#include "flow.h"
#include "nodeprocess.h"
#include <functional>
#include <optional>
namespace an {
class Runner;
class Coordinator;
enum class InputPolicy { Single, All, Any, Source };
struct SchemaInputs {
    std::vector<Json> ports;
    Json merged;
};
struct NodeDefinition {
    Kind kind;
    QString key, name;
    QStringList aliases;
    bool named = true;
    bool source = false, terminal = false, host = false, requiresModel = false;
    bool outputConnectionsRequired = false, editableOutputs = false, trailingOutput = false;
    QString unconnectedOutputMessage;
    bool schemaUsesInputs = true;
    InputPolicy inputPolicy = InputPolicy::Single;
    std::function<unsigned(const Node &)> inputs = [](const Node &) { return 1u; };
    std::function<unsigned(const Node &)> outputs = [](const Node &) { return 1u; };
    std::function<Json(const Node &, const SchemaInputs &)> schema = [](const Node &, const SchemaInputs &in) { return in.merged; };
    std::function<QStringList(const Flow &, const Node &)> validate = [](const Flow &, const Node &) { return QStringList{}; };
    std::function<QString(const Node &)> interpolation = [](const Node &) { return QString(); };
    std::function<QStringList(const Node &)> variables = [](const Node &) { return QStringList{}; };
    std::function<void(Node &)> initialize = [](Node &) {};
    std::function<bool(const Node &)> structured = [](const Node &) { return false; };
    std::function<QString(const Node &)> executionAgent = [](const Node &) { return QString(); };
    std::function<unsigned(const Node &)> editableOutputCount;
    std::function<bool(const Node &, unsigned, const Node &, unsigned)> sameOutput;
    void (Coordinator::*operate)(Runner *, const Node &, const QString &, const QString &,
                                 std::function<void(const Json &, const QString &)>) = nullptr;
    std::function<Json(const QString &)> hostFailure;
    void (Runner::*execute)(Id, const Json &) = nullptr;
    void (Runner::*prepare)(Id, const Json &, ProcessJob &, ProcessLaunch &) = nullptr;
    void (Runner::*planWait)(const Node &, PendingWait &, QString &) = nullptr;
    Json (Runner::*decode)(Id, ProcessJob &, int) = nullptr;
};
const std::vector<NodeDefinition> &nodeDefinitions();
const NodeDefinition &nodeDefinition(Kind kind);
Json schemaSources(const std::vector<Json> &values);
std::vector<int> remapNodeOutputs(const Node &old, const Node &node, std::optional<unsigned> removed);
struct NodeRegistry {
    static NodeDefinition start();
    static NodeDefinition end();
    static NodeDefinition agent();
    static NodeDefinition all();
    static NodeDefinition branch();
    static NodeDefinition script();
    static NodeDefinition shell();
    static NodeDefinition projectCreate();
    static NodeDefinition flowRun();
    static NodeDefinition flowWait();
    static NodeDefinition any();
    static NodeDefinition userInput();
    static NodeDefinition delay();
    static NodeDefinition schedule();
};
} // namespace an
