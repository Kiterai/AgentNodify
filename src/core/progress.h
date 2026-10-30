#pragma once
#include "runner.h"
#include "agents/common/progress.h"
#include <QMap>

namespace an {
// Interprets display data only. Never executes commands or accesses reported files.
class ProgressParser : private AgentProgressSink {
public:
    ProgressParser();
    void setDirectory(const QString &directory) { directory_ = directory; }
    void setPlainLog(bool plain) { plainLog_ = plain; }
    void append(const QString &chunk);
    void finish(State state);
    void finalResponse(const Json &output, bool structured = false) override;
    const std::vector<ProgressItem> &items() const { return items_; }
private:
    std::vector<ProgressItem> items_;
    QMap<QString, size_t> indexes_;
    std::vector<std::unique_ptr<AgentProgressProtocol>> protocols_;
    QString pending_, directory_;
    quint64 line_ = 0;
    bool pendingBoundary_ = false;
    bool plainLog_ = false;
    ProgressItem &item(const QString &key, ProgressKind kind) override;
    void line(const QString &text);
    void event(const Json &value);
    void tool(const QString &key, const QString &name, const QString &status, const Json *input, const Json *output) override;
    void files(const QString &key, const QString &name, const Json &input, const QString &status);
    void file(const QString &key, const QString &path, const QString &access, const QString &status) override;
    void simple(const Json &value, const QString &label = {}) override;
    void summarizeFileTool(const QString &key) override;
    void boundary() override { pendingBoundary_ = !items_.empty(); }
    quint64 lineNumber() const override { return line_; }
};
QString progressStatus(const QString &status);
// Strict JSON detection, with a depth guard for untrusted CLI output.
Json displayJson(const QString &text);
}
