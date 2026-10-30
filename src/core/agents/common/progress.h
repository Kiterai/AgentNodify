#pragma once
#include "core/flow.h"
#include <memory>

namespace an {
enum class ProgressKind { Message, Tool, FileAccess, Simple, Thinking, Prompt };
struct ProgressItem {
    QString key;
    ProgressKind kind = ProgressKind::Simple;
    QString name, status, text, path;
    Json content, input, output;
    bool hasInput = false, hasOutput = false;
    bool jsonContent = false;
    bool finalResponse = false;
    bool hidden = false;
    bool spaceBefore = false;
    QString commandLabel, command;
};
// Providers emit display data only; no process, filesystem or UI access.
class AgentProgressSink {
public:
    virtual ~AgentProgressSink() = default;
    virtual ProgressItem &item(const QString &, ProgressKind) = 0;
    virtual void simple(const Json &, const QString &label = {}) = 0;
    virtual void tool(const QString &, const QString &, const QString &, const Json *, const Json *) = 0;
    virtual void file(const QString &, const QString &, const QString &, const QString &) = 0;
    virtual void finalResponse(const Json &, bool structured = false) = 0;
    virtual void summarizeFileTool(const QString &) = 0;
    virtual void boundary() = 0;
    virtual quint64 lineNumber() const = 0;
};
class AgentProgressProtocol {
public:
    virtual ~AgentProgressProtocol() = default;
    virtual bool receive(const Json &, AgentProgressSink &) = 0;
    virtual QString fallbackKey(const Json &) const { return {}; }
    virtual QString commandLabel(const QString &, const QString &) const { return {}; }
    virtual bool summarizeFile(const QString &, const QString &) const { return false; }
};
QString agentStringValue(const Json &);
QString agentProgressStatus(const QString &);
QString agentFileAccess(const QString &);
Json expandedAgentInput(const Json &);
Json displayJson(const QString &text);
}
