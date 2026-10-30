#include "definition.h"
#include "codex/agent.h"
#include "claude/agent.h"
#include "opencode/agent.h"
#include <stdexcept>

namespace an {
const std::vector<AgentDefinition> &agentDefinitions() {
    // The only composition point for concrete agent implementations.
    static const std::vector<AgentDefinition> definitions{codexAgent(), claudeCodeAgent(), openCodeAgent()};
    return definitions;
}
const AgentDefinition *findAgentDefinition(const QString &name) {
    for (const auto &definition : agentDefinitions()) if (name == definition.name) return &definition;
    return nullptr;
}
const AgentDefinition &agentDefinition(const QString &name) {
    const auto *definition = findAgentDefinition(name);
    if (!definition) throw std::runtime_error(strings::UnknownAgent);
    return *definition;
}
QStringList agentNames() {
    QStringList names;
    for (const auto &definition : agentDefinitions()) names << definition.name;
    return names;
}
}
