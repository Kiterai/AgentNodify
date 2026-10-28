#include "shared/strings.h"
#include "nodedefinition.h"
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
    for (const auto &[id, node] : flow->second.nodes) if (nodeDefinition(node.kind).requiresModel && node.model.isEmpty()) return false;
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
        session->active = false; session->success = false; session->error = strings::TargetCancelled;
        auto waiters = std::move(session->waiters); session->waiters.clear(); detach(runner); callers_.erase(runner);
        for (const auto &waiter : waiters) if (waiter.caller) waiter.complete({}, session->error);
        emit sessionFinished(session->projectId, false, strings::ExecutionCancelled);
        emit activityChanged();
    });
    connect(runner, &Runner::finished, this, [this, runner, session](bool ok, const QString &message) {
        session->active = false; session->success = ok; session->error = strings::TargetFailed;
        bool terminal = false, hasEnd = false, failed = false;
        for (const auto &[id, node] : session->flow.nodes) {
            hasEnd = hasEnd || nodeDefinition(node.kind).terminal;
            auto result = runner->results().find(id);
            if (result != runner->results().end() && result->second.state == State::Cancelled) session->error = strings::TargetCancelled;
            if (result != runner->results().end() && result->second.state == State::Failed) failed = true;
            if (nodeDefinition(node.kind).terminal && result != runner->results().end() && result->second.state == State::Succeeded) { session->output = result->second.output; terminal = true; }
        }
        if (failed) session->error = strings::TargetFailed;
        if (ok && hasEnd && !terminal) { session->success = false; session->error = strings::TerminalNotReached; }
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
void Coordinator::failOperation(const Node &node, const QString &reason, Runner::HostCompletion complete) {
    const auto output = nodeDefinition(node.kind).hostFailure(reason);
    complete(output, output.is_null() ? reason : QString());
}
void Coordinator::operation(Runner *caller, Id, const Node &node, const QString &path, Runner::HostCompletion complete) {
    const auto failed = [&](const QString &reason) { failOperation(node, reason, complete); };
    if (!flows.contains(node.targetFlow)) { failed(strings::FlowNotFound); return; }
    if (path.trimmed().isEmpty() || path.contains(QChar(0))) { failed(strings::InvalidPath); return; }
    const auto directory = normalizedPath(path), identity = key(node.targetFlow, directory);
    (this->*nodeDefinition(node.kind).operate)(caller, node, directory, identity, std::move(complete));
}
}
