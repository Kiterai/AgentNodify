#pragma once
#include "runner.h"
#include <QPointer>
#include <optional>

namespace an {
// All calls run on the GUI/event-loop thread. Sessions outlive their callers;
// waiters bind to one invocation, never to the currently selected UI project.
class Coordinator : public QObject {
    Q_OBJECT
public:
    explicit Coordinator(QObject *parent = nullptr);
    ~Coordinator() override;
    struct Project { QString id, flowPath, directory; };
    std::map<QString, Flow> flows;
    std::vector<Project> projects;
    AgentConfig config;
    static QString normalizedPath(const QString &path);
    bool start(const Project &project, Runner *runner = nullptr);
    bool running() const;
    bool running(const QString &projectId) const;
    std::optional<Flow> activeFlow(const QString &projectId) const;
    void cancelProject(const QString &projectId);
    void cancelAll();
    bool submitUserInput(const QString &requestId, const QString &text, unsigned choice);
signals:
    void projectCreated(QString id, QString flowPath, QString directory);
    void executionChanged(QString projectId, const an::NodeExecution &execution);
    void resultChanged(QString projectId, unsigned int node, const an::Result &result);
    void activityChanged();
    void sessionStarted(QString projectId);
    void sessionFinished(QString projectId, bool success, QString message);
    void userInputRequested(QString projectId, const an::UserInputRequest &request);
    void userInputClosed(QString requestId);
private:
    struct Waiter { QPointer<Runner> caller; Runner::HostCompletion complete; };
    struct Session {
        QString projectId;
        Flow flow;
        QPointer<Runner> runner;
        bool active = false, success = false;
        Json output;
        QString error;
        std::vector<Waiter> waiters;
    };
    std::map<QString, std::shared_ptr<Session>> sessions_;
    std::map<Runner *, QString> callers_;
    bool stopping_ = false;
    std::map<QString, QPointer<Runner>> inputOwners_;
    QString key(const QString &flow, const QString &directory) const;
    void operation(Runner *caller, Id node, const Node &target, const QString &path, Runner::HostCompletion complete);
    void detach(Runner *caller);
    bool reaches(const QString &from, const QString &to, std::set<QString> &seen) const;
};
}
