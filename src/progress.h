#pragma once
#include "runner.h"
#include <QMap>

namespace an {
enum class ProgressKind { Message, Tool, FileAccess, Simple };
struct ProgressItem {
    QString key;
    ProgressKind kind = ProgressKind::Simple;
    QString name, status, text, path;
    Json content, input, output;
    bool hasInput = false, hasOutput = false;
    bool jsonContent = false;
    bool finalResponse = false;
};
// Interprets display data only. Never executes commands or accesses reported files.
class ProgressParser {
public:
    void setDirectory(const QString &directory) { directory_ = directory; }
    void append(const QString &chunk);
    void finish(State state);
    void finalResponse(const Json &output, bool structured = false);
    const std::vector<ProgressItem> &items() const { return items_; }
private:
    struct Block { QString key, input; bool tool = false; };
    std::vector<ProgressItem> items_;
    QMap<QString, size_t> indexes_;
    QMap<int, Block> blocks_;
    QString pending_, claudeMessage_, directory_;
    int claudeBlock_ = -1;
    quint64 line_ = 0;
    ProgressItem &item(const QString &key, ProgressKind kind);
    void line(const QString &text);
    void event(const Json &value);
    void claude(const Json &value);
    void tool(const QString &key, const QString &name, const QString &status, const Json *input, const Json *output);
    void files(const QString &key, const QString &name, const Json &input, const QString &status);
    void file(const QString &key, const QString &path, const QString &access, const QString &status);
    void simple(const Json &value, const QString &label = {});
};
QString progressStatus(const QString &status);
// Strict JSON detection, with a depth guard for untrusted CLI output.
Json displayJson(const QString &text);
}
