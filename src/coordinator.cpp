#include "coordinator.h"
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>

namespace an {
Coordinator::Coordinator(QObject *parent) : QObject(parent) {}
Coordinator::~Coordinator() {
    disconnect(this, nullptr, nullptr, nullptr);
    for (auto &[key, session] : sessions_) if (session->runner) {
        session->runner->disconnect(this); session->runner->hostHandler = {}; session->runner->cancelHostOperations = {};
        session->runner->cancel();
    }
}
QString Coordinator::normalizedPath(const QString &path) {
    if (path.isEmpty()) return {};
    QFileInfo info(QDir::cleanPath(QDir::fromNativeSeparators(path)));
    auto canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) return canonical;
    // Resolve existing symlink ancestors even before a new directory exists.
    const auto parent = info.absolutePath();
    if (parent != info.absoluteFilePath()) return QDir(normalizedPath(parent)).filePath(info.fileName());
    return info.absoluteFilePath();
}
QString Coordinator::key(const QString &flow, const QString &directory) const {
    auto result = normalizedPath(flow) + '\n' + normalizedPath(directory);
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
bool Coordinator::running() const {
    for (const auto &[key, session] : sessions_) if (session->active) return true;
    return false;
}
bool Coordinator::running(const QString &projectId) const {
    for (const auto &[key, session] : sessions_) if (session->active && session->projectId == projectId) return true;
    return false;
}
std::optional<Flow> Coordinator::activeFlow(const QString &projectId) const {
    for (const auto &[key, session] : sessions_) if (session->active && session->projectId == projectId) return session->flow;
    return {};
}
void Coordinator::cancelProject(const QString &projectId) {
    const auto id = projectId;
    for (const auto &[key, session] : sessions_) if (session->active && session->projectId == id && session->runner) session->runner->cancel();
}
void Coordinator::cancelAll() {
    stopping_ = true;
    // Cancellation callbacks can detach waiters, but never erase sessions.
    for (const auto &[key, session] : sessions_) if (session->active && session->runner) session->runner->cancel();
    stopping_ = false;
}
bool Coordinator::submitUserInput(const QString &requestId, const QString &text, unsigned choice) {
    const auto owner = inputOwners_.find(requestId);
    return owner != inputOwners_.end() && owner->second && owner->second->submitUserInput(requestId, text, choice);
}
void Coordinator::detach(Runner *caller) {
    for (auto &[key, session] : sessions_)
        std::erase_if(session->waiters, [caller](const Waiter &waiter) { return !waiter.caller || waiter.caller == caller; });
}
bool Coordinator::reaches(const QString &from, const QString &to, std::set<QString> &seen) const {
    if (from == to) return true;
    if (!seen.insert(from).second) return false;
    for (const auto &[target, session] : sessions_) if (session->active)
        for (const auto &waiter : session->waiters) if (waiter.caller) {
            const auto caller = callers_.find(waiter.caller);
            if (caller != callers_.end() && caller->second == from && reaches(target, to, seen)) return true;
        }
    return false;
}
bool Coordinator::start(const Project &project, Runner *runner) {
    const Project target = project; // Registry updates from signals may reallocate projects.
    if (runner && (runner->running() || callers_.contains(runner))) return false;
    if (running(target.id)) return false;
    const auto identity = key(project.flowPath, project.directory);
    if (stopping_ || (sessions_.contains(identity) && sessions_.at(identity)->active)) return false;
    auto flow = flows.find(project.flowPath);
    if (flow == flows.end() || !QFileInfo(project.directory).isDir() || !flow->second.validate().isEmpty()) return false;
    for (const auto &[id, node] : flow->second.nodes) if (node.kind == Kind::Agent && node.model.isEmpty()) return false;
    const Flow snapshot = flow->second; const AgentConfig settings = config;
    if (!runner) runner = new Runner(this);
    auto session = std::make_shared<Session>(); session->projectId = target.id; session->flow = snapshot; session->runner = runner; session->active = true;
    sessions_[identity] = session; callers_[runner] = identity;
    runner->hostHandler = [this](Runner *caller, Id id, const Node &node, const QString &path, Runner::HostCompletion done) { operation(caller, id, node, path, std::move(done)); };
    runner->cancelHostOperations = [this](Runner *caller) { detach(caller); };
    connect(runner, &Runner::userInputRequested, this, [this, runner, id = project.id](const UserInputRequest &request) {
        inputOwners_[request.id] = runner; emit userInputRequested(id, request);
    });
    connect(runner, &Runner::userInputClosed, this, [this](const QString &id) { inputOwners_.erase(id); emit userInputClosed(id); });
    connect(runner, &Runner::executionChanged, this, [this, id = project.id](const NodeExecution &entry) { emit executionChanged(id, entry); });
    connect(runner, &Runner::changed, this, [this, runner, id = project.id](Id node) { if (runner->results().contains(node)) emit resultChanged(id, node, runner->results().at(node)); });
    connect(runner, &QObject::destroyed, this, [this, runner, session] {
        if (!session->active) return;
        session->active = false; session->success = false; session->error = "target_cancelled";
        auto waiters = std::move(session->waiters); session->waiters.clear(); detach(runner); callers_.erase(runner);
        for (const auto &waiter : waiters) if (waiter.caller) waiter.complete({}, session->error);
        emit sessionFinished(session->projectId, false, "実行を中止しました");
        emit activityChanged();
    });
    connect(runner, &Runner::finished, this, [this, runner, session](bool ok, const QString &message) {
        session->active = false; session->success = ok; session->error = "target_failed";
        bool terminal = false, hasEnd = false, failed = false;
        for (const auto &[id, node] : session->flow.nodes) {
            hasEnd = hasEnd || node.kind == Kind::End;
            auto result = runner->results().find(id);
            if (result != runner->results().end() && result->second.state == State::Cancelled) session->error = "target_cancelled";
            if (result != runner->results().end() && result->second.state == State::Failed) failed = true;
            if (node.kind == Kind::End && result != runner->results().end() && result->second.state == State::Succeeded) { session->output = result->second.output; terminal = true; }
        }
        if (failed) session->error = "target_failed";
        if (ok && hasEnd && !terminal) { session->success = false; session->error = "terminal_not_reached"; }
        auto waiters = std::move(session->waiters); session->waiters.clear(); detach(runner); callers_.erase(runner);
        for (const auto &waiter : waiters) if (waiter.caller) waiter.complete(session->output, session->success ? QString() : session->error);
        runner->disconnect(this);
        if (runner->parent() == this) runner->deleteLater();
        emit sessionFinished(session->projectId, ok, message);
        emit activityChanged();
    });
    emit sessionStarted(target.id);
    const bool started = runner->start(snapshot, target.directory, settings, target.flowPath);
    emit activityChanged(); return started;
}
void Coordinator::operation(Runner *caller, Id, const Node &node, const QString &path, Runner::HostCompletion complete) {
    const auto failed = [&](const QString &reason) {
        if (node.kind == Kind::ProjectCreate) complete(Json{{"result", "failed"}, {"reason", reason.toStdString()}}, {});
        else if (node.kind == Kind::FlowRun) complete(Json{{"result", "failed"}}, {});
        else complete({}, reason);
    };
    if (!flows.contains(node.targetFlow)) { failed("flow_not_found"); return; }
    if (path.trimmed().isEmpty() || path.contains(QChar(0))) { failed("invalid_path"); return; }
    const auto directory = normalizedPath(path), identity = key(node.targetFlow, directory);
    auto project = std::find_if(projects.begin(), projects.end(), [&](const Project &p) { return key(p.flowPath, p.directory) == identity; });
    if (node.kind == Kind::ProjectCreate) {
        if (project != projects.end()) { failed("project_already_exists"); return; }
        if (QFileInfo::exists(directory) && !QFileInfo(directory).isDir()) { failed("path_not_directory"); return; }
        if (!QDir().mkpath(directory)) { failed("directory_create_failed"); return; }
        Project created{QUuid::createUuid().toString(QUuid::WithoutBraces), node.targetFlow, normalizedPath(directory)};
        projects.push_back(created); emit projectCreated(created.id, created.flowPath, created.directory);
        complete(Json{{"result", "success"}, {"path", created.directory.toStdString()}}, {}); return;
    }
    if (project == projects.end()) { failed("project_not_found"); return; }
    if (node.kind == Kind::FlowRun) {
        if (sessions_.contains(identity) && sessions_.at(identity)->active) { complete(Json{{"result", "already"}}, {}); return; }
        // Copy: launching can indirectly grow the project registry.
        const auto target = *project;
        complete(Json{{"result", start(target) ? "started" : "failed"}}, {}); return;
    }
    auto found = sessions_.find(identity);
    if (found == sessions_.end()) { failed("execution_not_started"); return; }
    const auto session = found->second;
    if (!session->active) { complete(session->output, session->success ? QString() : session->error); return; }
    std::set<QString> seen;
    if (!callers_.contains(caller) || reaches(identity, callers_.at(caller), seen)) { failed("wait_cycle"); return; }
    session->waiters.push_back({caller, std::move(complete)});
}
}
