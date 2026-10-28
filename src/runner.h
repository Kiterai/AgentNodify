#pragma once
#include "strings.h"
#include "nodeprocess.h"
#include "flow.h"
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QStringDecoder>
#include <QDateTime>
#include <QMetaType>
#include <deque>
#include <memory>
#include <functional>
#include <set>

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
    quint64 executionNumber = 0;
    QString agent, directory;
    QString model, effort;
    QDateTime startedAt, finishedAt;
    State state = State::Running;
    QString log;
    Json output;
    quint64 sequence = 0; // Assigned by the project's persistent history.
    bool structuredOutput = false;
    bool retainContext = false, contextResumed = false;
};
struct AgentConfig {
    QString codex = strings::CodexExecutable;
    QString claude = strings::ClaudeExecutable;
#ifdef Q_OS_WIN
    QString opencode = strings::OpenCodeWindowsExecutable;
#else
    QString opencode = strings::OpenCodeExecutable;
#endif
    bool opencodeStandalone = false;
};
struct UserInputRequest {
    QString id;
    Id node = 0;
    Json message;
    QStringList choices;
};
// Every firing enqueues an event. Ordinary nodes consume one event; All consumes
// exactly one event from every input FIFO, then waits for a fresh complete set.
// Any consumes one event from the oldest nonempty input FIFO across all ports.
class Runner : public QObject {
    Q_OBJECT
public:
    explicit Runner(QObject *parent = nullptr, std::function<QDateTime()> clock = {});
    ~Runner() override;
    bool start(const Flow &flow, const QString &directory, const AgentConfig &config, const QString &flowPath = {});
    void cancel();
    bool submitUserInput(const QString &requestId, const QString &text, unsigned choice);
    bool running() const { return active_; }
    const std::map<Id, Result> &results() const { return results_; }
    using HostCompletion = std::function<void(const Json &, const QString &)>;
    using HostHandler = std::function<void(Runner *, Id, const Node &, const QString &, HostCompletion)>;
    HostHandler hostHandler;
    std::function<void(Runner *)> cancelHostOperations;
signals:
    void executionChanged(const an::NodeExecution &execution);
    void changed(unsigned int id);
    void fired(unsigned int id, quint64 execution, QString outputJson);
    void finished(bool success, QString message);
    void userInputRequested(const an::UserInputRequest &request);
    void userInputClosed(QString requestId);
private:
    friend struct NodeRegistry;
    using Job = ProcessJob;
    Flow flow_;
    AgentConfig config_;
    QString directory_, flowDirectory_;
    std::map<Id, Result> results_;
    std::map<Id, QString> agentSessions_;
    std::map<Id, NodeExecution> activeExecutions_;
    struct Token { Json value; quint64 arrival; };
    std::map<Id, std::vector<std::deque<Token>>> tokens_;
    quint64 arrival_ = 0;
    std::map<Id, std::unique_ptr<Job>> jobs_;
    std::set<Id> pendingHost_;
    struct PendingInput { UserInputRequest request; bool submitted = false; };
    std::map<Id, PendingInput> pendingInput_;
    std::map<Id, PendingWait> pendingWait_;
    bool active_ = false;
    bool pumping_ = false;
    bool pumpScheduled_ = false;
    bool startPending_ = false;
    quint64 generation_ = 0;
    std::function<QDateTime()> clock_;
    QDateTime now() const;
    void prepareAgent(Id, const Json &, ProcessJob &, ProcessLaunch &);
    Json decodeAgent(Id, ProcessJob &, int);
    void prepareScript(Id, const Json &, ProcessJob &, ProcessLaunch &);
    Json decodeScript(Id, ProcessJob &, int);
    void prepareShell(Id, const Json &, ProcessJob &, ProcessLaunch &);
    Json decodeShell(Id, ProcessJob &, int);
    void executeImmediate(Id, const Json &);
    void executeUserInput(Id, const Json &);
    void executeHost(Id, const Json &);
    void executeBranch(Id, const Json &);
    void planDelay(const Node &, PendingWait &, QString &);
    void planSchedule(const Node &, PendingWait &, QString &);
    void pump();
    void schedulePump();
    void complete(Id id, const Json &output, int selected = -1);
    void launch(Id id, const Json &input);
    void launchWait(Id id, const Json &input);
    void checkWait(Id id, quint64 generation);
    void processFinished(Id id, int code, QProcess::ExitStatus status);
    void fail(Id id, const QString &message);
    void stopJobs();
    void appendLog(Id id, const QByteArray &bytes);
    void beginExecution(Id id);
    void finishExecution(Id id);
};
// Exposed for protocol fixture tests. Never treat process output as commands.
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured);
QString decodeAgentSessionId(const QString &agent, const QByteArray &data);
}
Q_DECLARE_METATYPE(an::NodeExecution)
Q_DECLARE_METATYPE(an::UserInputRequest)
