#pragma once
#include "config.h"
#include "common/catalog.h"
#include "common/progress.h"
#include "core/nodeprocess.h"
#include <functional>

namespace an {
struct AgentTask {
    const Node &node;
    const AgentConfig &config;
    QString directory, prompt, session;
};
struct AgentOutput {
    QString text, finalText;
    Json structuredValue;
    bool hasStructured = false;
};
struct AgentDefinition {
    const char *name = nullptr;
    const char *configKey = nullptr;
    const char *defaultProgram = nullptr;
    QString AgentConfig::*legacyExecutable = nullptr;
    std::function<QString(const QString &)> migrateProgram;
    QStringList fallbackEfforts;
    bool promptOnStdin = true;
    const char *sessionPattern = nullptr;
    std::function<QString(const AgentTask &, ProcessJob &, ProcessLaunch &)> prepare;
    std::function<void(const Json &, AgentOutput &)> outputEvent;
    std::function<const char *(const Json &)> sessionField;
    std::function<std::unique_ptr<AgentCatalogProtocol>()> catalog;
    std::function<std::unique_ptr<AgentProgressProtocol>()> progress;
};
const std::vector<AgentDefinition> &agentDefinitions();
const AgentDefinition *findAgentDefinition(const QString &name);
const AgentDefinition &agentDefinition(const QString &name);
QStringList agentNames();
QString agentExecutable(const AgentDefinition &, const AgentConfig &);
void setAgentExecutable(const AgentDefinition &, AgentConfig &, const QString &);
Json agentConfigJson(const AgentConfig &);
AgentConfig agentConfigFromJson(const Json &);
QString prepareAgentLaunch(const AgentTask &, ProcessJob &, ProcessLaunch &);
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured);
QString decodeAgentSessionId(const QString &agent, const QByteArray &data);
Json readAgentResponse(const QString &agent, const ProcessJob &, bool structured);
}
