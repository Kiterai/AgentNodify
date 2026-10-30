#include "core/agents/definition.h"

namespace an {
QString agentExecutable(const AgentDefinition &definition, const AgentConfig &config) {
    return definition.legacyExecutable ? config.*definition.legacyExecutable
        : config.additionalExecutables.value(definition.configKey, definition.defaultProgram);
}
void setAgentExecutable(const AgentDefinition &definition, AgentConfig &config, const QString &program) {
    if (definition.legacyExecutable) config.*definition.legacyExecutable = program;
    else config.additionalExecutables[definition.configKey] = program;
}
Json agentConfigJson(const AgentConfig &config) {
    Json value = Json::object();
    for (const auto &definition : agentDefinitions())
        value[definition.configKey] = agentExecutable(definition, config).toStdString();
    return value;
}
AgentConfig agentConfigFromJson(const Json &value) {
    AgentConfig config;
    for (const auto &definition : agentDefinitions()) {
        if (!value.contains(definition.configKey)) continue;
        auto program = QString::fromStdString(value.at(definition.configKey).get<std::string>());
        if (definition.migrateProgram) program = definition.migrateProgram(program);
        setAgentExecutable(definition, config, program);
    }
    return config;
}
}
