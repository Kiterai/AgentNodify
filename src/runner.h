#pragma once
#include "flow.h"
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QStringDecoder>
#include <QDateTime>
#include <QMetaType>
#include <deque>
#include <memory>

namespace an {
enum class State { Pending, Running, Succeeded, Skipped, Failed, Cancelled };
QString stateName(State state);
struct Result {
    State state = State::Pending;
    QString log;
    Json output;
    quint64 executions = 0;
};
struct NodeExecution {
    QString id;
    Id node = 0;
    QString title;
    QDateTime startedAt, finishedAt;
    State state = State::Running;
    QString log;
    Json output;
    quint64 sequence = 0; // Assigned by the project's persistent history.
    bool structuredOutput = false;
};
struct AgentConfig {
    QString codex = "codex";
    QString claude = "claude";
#ifdef Q_OS_WIN
    QString opencode = "opencode-cli";
#else
    QString opencode = "opencode";
#endif
    int timeoutSeconds = 600;
    bool opencodeStandalone = false;
    bool workspaceWrite = false;
};
// Every firing enqueues an event. Ordinary nodes consume one event; AND consumes
// exactly one event from every input FIFO, then waits for a fresh complete set.
class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr);
    ~Runner() override;
    bool start(const Flow &flow, const QString &directory, const AgentConfig &config);
    void cancel();
    bool running() const { return active_; }
    const std::map<Id, Result> &results() const { return results_; }
signals:
    void executionChanged(const an::NodeExecution &execution);
    void changed(unsigned int id);
    void fired(unsigned int id, quint64 execution, QString outputJson);
    void finished(bool success, QString message);
private:
    struct Job {
        QProcess *process = nullptr;
        QTimer *timer = nullptr;
        QByteArray stdoutData;
        QStringDecoder stdoutDecoder{QStringDecoder::Utf8}, stderrDecoder{QStringDecoder::Utf8};
        QString finalPath;
        std::unique_ptr<QTemporaryDir> temp;
    };
    Flow flow_;
    AgentConfig config_;
    QString directory_;
    std::map<Id, Result> results_;
    std::map<Id, NodeExecution> activeExecutions_;
    std::map<Id, std::vector<std::deque<Json>>> tokens_;
    std::map<Id, std::unique_ptr<Job>> jobs_;
    bool active_ = false;
    bool pumping_ = false;
    bool pumpScheduled_ = false;
    bool startPending_ = false;
    quint64 generation_ = 0;
    void pump();
    void schedulePump();
    void complete(Id id, const Json &output, int selected = -1);
    void launch(Id id, const Json &input);
    void processFinished(Id id, int code, QProcess::ExitStatus status);
    void fail(Id id, const QString &message);
    void stopJobs();
    void appendLog(Id id, const QByteArray &bytes);
    void beginExecution(Id id);
    void finishExecution(Id id);
};
// Exposed for protocol fixture tests. Never treat process output as commands.
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured);
}
Q_DECLARE_METATYPE(an::NodeExecution)
