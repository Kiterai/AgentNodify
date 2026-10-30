#include "shared/constants.h"
#include "shared/strings.h"
#include "nodedefinition.h"
#include "runner.h"
#include "cli.h"
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTimer>
#include <QRegularExpression>
#include <QUuid>
#include <stdexcept>

namespace an {
QString stateName(State s) {
    switch(s) {
    case State::Pending: return strings::Waiting; case State::Running: return strings::Running;
    case State::Succeeded: return strings::Succeeded; case State::Skipped: return strings::Skipped;
    case State::Cancelled: return strings::Cancelled; default: return strings::Error;
    }
}
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured) {
    QString text, finalText; Json structuredValue; bool hasStructured = false;
    for (auto &line : data.split('\n')) {
        if (line.trimmed().isEmpty()) continue;
        auto j = Json::parse(line.toStdString(), nullptr, false);
        if (j.is_discarded()) throw std::runtime_error(strings::InvalidCliJson);
        if (j.value(strings::IsError, false) || j.value(strings::Type, strings::Empty) == strings::ErrorValue) throw std::runtime_error(j.dump());
        if (agent == strings::ClaudeCode) {
            if (j.value(strings::Type, strings::Empty) == strings::Result) {
                if (j.contains(strings::StructuredOutputValue)) { structuredValue = j[strings::StructuredOutputValue]; hasStructured = true; }
                if (j.contains(strings::Result) && j[strings::Result].is_string()) finalText = QString::fromStdString(j[strings::Result].get<std::string>());
            } else if (j.value(strings::Type, strings::Empty) == strings::Assistant && j.contains(strings::Message)) {
                for (auto &c : j[strings::Message].value(strings::Content, Json::array()))
                    if (c.value(strings::Type, strings::Empty) == strings::TextFormat) text += QString::fromStdString(c.value(strings::TextFormat, strings::Empty));
            }
        } else if (agent == strings::OpenCode) {
            if (j.value(strings::Type, strings::Empty) == strings::TextFormat && j.contains(strings::Part)) text += QString::fromStdString(j[strings::Part].value(strings::TextFormat, strings::Empty));
        } else if (j.value(strings::Type, strings::Empty) == strings::ItemCompleted && j.contains(strings::Item) && j[strings::Item].value(strings::Type, strings::Empty) == strings::AgentMessage) {
            finalText = QString::fromStdString(j[strings::Item].value(strings::TextFormat, strings::Empty));
        }
    }
    if (structured && hasStructured) return structuredValue;
    if (!finalText.isEmpty()) text = finalText;
    if (text.isEmpty()) throw std::runtime_error(strings::MissingAgentResponse);
    return structured ? parseJson(text.trimmed()) : Json(text.toUtf8().toStdString());
}
QString decodeAgentSessionId(const QString &agent, const QByteArray &data) {
    QString session;
    const QRegularExpression uuid(strings::UuidPattern);
    const QRegularExpression openCodeId(strings::OpenCodeSessionPattern);
    for (const auto &line : data.split('\n')) {
        const auto event = Json::parse(line.toStdString(), nullptr, false);
        if (!event.is_object()) continue;
        const auto type = event.value(strings::Type, Json());
        const char *field = nullptr;
        if (agent == strings::Codex && type == strings::ThreadStarted) field = strings::ThreadId;
        else if (agent == strings::ClaudeCode && (type == strings::Result || (type == strings::System && event.value(strings::Subtype, Json()) == strings::Init))) field = strings::SessionId;
        else if (agent == strings::OpenCode && (type == strings::StepStart || type == strings::StepFinish || type == strings::TextFormat || type == strings::ToolUse)) field = strings::SessionID;
        if (!field || !event.contains(field)) continue;
        if (!event.at(field).is_string()) throw std::runtime_error(strings::InvalidSessionId);
        const auto id = QString::fromStdString(event.at(field).get<std::string>());
        if (!(agent == strings::OpenCode ? openCodeId : uuid).match(id).hasMatch()) throw std::runtime_error(strings::InvalidSessionId);
        if (!session.isEmpty() && session != id) throw std::runtime_error(strings::ConflictingSessionIds);
        session = id;
    }
    return session;
}
Runner::Runner(QObject *parent, std::function<QDateTime()> clock) : QObject(parent), clock_(std::move(clock)) {}
QDateTime Runner::now() const { return clock_ ? clock_() : QDateTime::currentDateTime(); }
Runner::~Runner() { active_ = false; stopJobs(); }
bool Runner::start(const Flow &flow, const QString &directory, const AgentConfig &config, const QString &flowPath) {
    if (active_) return false;
    auto errors = flow.validate();
    if (!QFileInfo(directory).isDir()) errors << strings::MissingProjectDirectory;
    if (!errors.isEmpty()) { emit finished(false, errors.join('\n')); return false; }
    flow_ = flow; directory_ = QFileInfo(directory).canonicalFilePath(); config_ = config;
    const auto source = flowPath.isEmpty() ? flow.sourcePath : flowPath;
    flowDirectory_ = source.isEmpty() ? QString() : QFileInfo(source).absolutePath();
    results_.clear(); tokens_.clear(); jobs_.clear(); activeExecutions_.clear(); agentSessions_.clear(); arrival_ = 0;
    ++generation_; active_ = true; pumpScheduled_ = false; startPending_ = true;
    for (auto &[id, n] : flow_.nodes) {
        tokens_[id].resize(flow_.inputCount(n)); results_[id] = {}; emit changed(id);
    }
    schedulePump(); return true;
}
void Runner::appendLog(Id id, const QByteArray &bytes) {
    auto &log = results_[id].log;
    log += QString::fromUtf8(bytes);
    constexpr qsizetype limit = limits::LogCharacters;
    if (log.size() > limit) log = strings::TruncatedLogNotice + log.right(limit);
    if (activeExecutions_.contains(id)) {
        activeExecutions_[id].log = log;
        emit executionChanged(activeExecutions_.at(id));
    }
    emit changed(id);
}
void Runner::beginExecution(Id id) {
    auto &r = results_[id]; ++r.executions; r.state = State::Running; r.log.clear(); r.output = nullptr;
    NodeExecution execution; execution.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    execution.node = id; execution.title = flow_.nodes.at(id).title;
    execution.executionNumber = r.executions; execution.directory = directory_;
    const auto &node = flow_.nodes.at(id); const auto &definition = nodeDefinition(node.kind);
    execution.agent = definition.executionAgent(node);
    execution.structuredOutput = definition.structured(node);
    if (definition.requiresModel) {
        execution.model = node.model; execution.effort = node.effort;
        execution.retainContext = node.retainContext;
        execution.contextResumed = execution.retainContext && agentSessions_.contains(id);
    }
    if (execution.title.isEmpty()) execution.title = kindName(flow_.nodes.at(id).kind);
    execution.startedAt = QDateTime::currentDateTimeUtc(); activeExecutions_[id] = execution;
    emit executionChanged(activeExecutions_.at(id));
    if (active_) emit changed(id);
}
void Runner::finishExecution(Id id) {
    auto it = activeExecutions_.find(id); if (it == activeExecutions_.end()) return;
    auto execution = std::move(it->second); activeExecutions_.erase(it);
    const auto &r = results_.at(id); execution.state = r.state; execution.log = r.log; execution.output = r.output;
    execution.finishedAt = QDateTime::currentDateTimeUtc(); emit executionChanged(execution);
}
void Runner::stopJobs() {
    for (auto &[id, wait] : pendingWait_) { wait.timer->stop(); wait.timer->disconnect(this); wait.timer->deleteLater(); }
    pendingWait_.clear();
    auto pending = std::move(pendingInput_); pendingInput_.clear();
    for (const auto &[id, input] : pending) emit userInputClosed(input.request.id);
    agentSessions_.clear();
    pendingHost_.clear();
    if (cancelHostOperations) cancelHostOperations(this);
    for (auto &[id, j] : jobs_) {
        j->process->disconnect(this);
        if (j->process->state() != QProcess::NotRunning) { j->process->kill(); j->process->waitForFinished(time::ProcessKillMilliseconds); }
        auto &log = results_[id].log;
        const auto remaining = j->process->readAllStandardOutput();
        if (j->eventsOutput) log += QString(j->stdoutDecoder.decode(remaining));
        log += QString(j->stderrDecoder.decode(j->process->readAllStandardError()));
        constexpr qsizetype limit = limits::LogCharacters;
        if (log.size() > limit) log = strings::TruncatedLogNotice + log.right(limit);
        j->process->deleteLater();
    }
    jobs_.clear();
}
bool Runner::submitUserInput(const QString &requestId, const QString &text, unsigned choice) {
    if (!active_) return false;
    for (auto &[id, pending] : pendingInput_) if (pending.request.id == requestId) {
        if (pending.submitted || choice >= unsigned(pending.request.choices.size())) return false;
        pending.submitted = true;
        const auto generation = generation_;
        QTimer::singleShot(0, this, [this, generation, requestId, id, text, choice] {
            if (!active_ || generation_ != generation || !pendingInput_.contains(id) || pendingInput_.at(id).request.id != requestId) return;
            pendingInput_.erase(id); emit userInputClosed(requestId);
            if (!active_ || generation_ != generation) return;
            complete(id, Json(text.toUtf8().toStdString()), int(choice)); schedulePump();
        });
        return true;
    }
    return false;
}
void Runner::cancel() {
    if (!active_) return;
    active_ = false; stopJobs();
    for (auto &[id, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; finishExecution(id); emit changed(id); }
    emit finished(false, strings::ExecutionCancelled);
}
void Runner::fail(Id id, const QString &message) {
    if (!active_) return;
    active_ = false; results_[id].state = State::Failed; stopJobs();
    results_[id].log += strings::Newline + message; finishExecution(id); emit changed(id);
    for (auto &[other, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; finishExecution(other); emit changed(other); }
    const auto &node = flow_.nodes.at(id);
    emit finished(false, (node.title.isEmpty() ? kindName(node.kind) : node.title) + strings::ColonSpace + message);
}
void Runner::complete(Id id, const Json &output, int selected) {
    auto &r = results_[id]; r.state = State::Succeeded;
    r.output = output; finishExecution(id); emit changed(id);
    emit fired(id, r.executions, QString::fromStdString(output.dump()));
    if (!active_) return; // A consumer may cancel from a progress signal.
    for (auto &e : flow_.edges) if (e.from == id) {
        if (selected < 0 || int(e.output) == selected) tokens_[e.to][e.input].push_back({output, ++arrival_});
    }
}
void Runner::schedulePump() {
    if (!active_ || pumpScheduled_) return;
    pumpScheduled_ = true;
    const auto generation = generation_;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation != generation_) return;
        pumpScheduled_ = false; pump();
    });
}
void Runner::pump() {
    if (!active_ || pumping_) return;
    pumping_ = true;
    bool progress = false; unsigned budget = limits::PumpFirings;
    do {
        progress = false;
        for (auto &[id, n] : flow_.nodes) {
            if (!active_ || budget == 0) break;
            if (jobs_.contains(id) || pendingHost_.contains(id) || pendingInput_.contains(id) || pendingWait_.contains(id)) continue; // Serial per node, FIFO while busy.
            auto &inputs = tokens_.at(id);
            bool ready = nodeDefinition(n.kind).source ? startPending_ : true;
            if (nodeDefinition(n.kind).inputPolicy == InputPolicy::Any) {
                ready = false;
                for (const auto &queue : inputs) if (!queue.empty()) ready = true;
            } else for (auto &queue : inputs) if (queue.empty()) ready = false;
            if (!ready) continue;
            --budget;
            progress = true;
            Json input;
            if (nodeDefinition(n.kind).inputPolicy == InputPolicy::All) {
                input = Json::array();
                // Pop the complete set BEFORE publishing, including self-loops.
                for (auto &queue : inputs) { input.push_back(std::move(queue.front().value)); queue.pop_front(); }
            } else if (nodeDefinition(n.kind).inputPolicy == InputPolicy::Any) {
                unsigned selected = unsigned(inputs.size());
                for (unsigned i = 0; i < inputs.size(); ++i) if (!inputs[i].empty() && (selected == inputs.size() || inputs[i].front().arrival < inputs[selected].front().arrival)) selected = i;
                input = {{strings::Event, selected}, {strings::Input, std::move(inputs[selected].front().value)}};
                inputs[selected].pop_front();
            } else if (nodeDefinition(n.kind).source) { input = Json::object(); startPending_ = false; }
            else { input = std::move(inputs.at(0).front().value); inputs.at(0).pop_front(); }
            beginExecution(id);
            if (!active_) break;
            try {
                (this->*nodeDefinition(n.kind).execute)(id, input);
            } catch (const std::exception &e) { fail(id, QString::fromUtf8(e.what())); }
        }
    } while (progress && active_ && budget > 0);
    pumping_ = false;
    if (!active_) return;
    // Even a synchronous infinite loop must yield to cancellation / GUI events.
    if (budget == 0) { schedulePump(); return; }
    if (!jobs_.empty() || !pendingHost_.empty() || !pendingInput_.empty() || !pendingWait_.empty()) return;
    const auto disconnected=flow_.disconnectedInputs();
    for (auto &[id, inputs] : tokens_) for (auto &queue : inputs) if (!queue.empty() && !disconnected.contains(id)) {
        fail(id, strings::IncompleteAllInputs); return;
    }
    for (auto &[id, r] : results_) if (r.executions == 0) { r.state = State::Skipped; emit changed(id); }
    active_ = false; agentSessions_.clear();
    emit finished(true, strings::FlowCompleted);
}
void Runner::executeImmediate(Id id, const Json &input) { complete(id, input); }
void Runner::launchWait(Id id, const Json &input) {
    const auto &node=flow_.nodes.at(id); PendingWait wait; wait.input=input;
    QString message;
    (this->*nodeDefinition(node.kind).planWait)(node, wait, message);
    wait.timer=new QTimer(this); wait.timer->setSingleShot(true); wait.timer->setTimerType(Qt::PreciseTimer);
    const auto generation=generation_;
    connect(wait.timer,&QTimer::timeout,this,[this,id,generation]{checkWait(id,generation);});
    pendingWait_[id]=std::move(wait); appendLog(id,message.toUtf8());
    if(active_ && pendingWait_.contains(id)) {
        const auto &pending=pendingWait_.at(id);
        const qint64 remaining=pending.target.isValid()?now().msecsTo(pending.target):pending.duration-pending.elapsed.elapsed();
        pending.timer->start(int(qBound(qint64(0),remaining,qint64(time::WaitPollMilliseconds))));
    }
}
void Runner::checkWait(Id id, quint64 generation) {
    if(!active_ || generation_!=generation || !pendingWait_.contains(id)) return;
    auto &wait=pendingWait_.at(id);
    const qint64 remaining=wait.target.isValid()?now().msecsTo(wait.target):wait.duration-wait.elapsed.elapsed();
    if(remaining>0) { wait.timer->start(int(qMin(remaining,qint64(time::WaitPollMilliseconds)))); return; }
    const auto input=std::move(wait.input); wait.timer->deleteLater(); pendingWait_.erase(id);
    complete(id,input); schedulePump();
}
void Runner::launch(Id id, const Json &input) {
    const auto &n = flow_.nodes.at(id);
    auto j = std::make_unique<Job>();
    ProcessLaunch launch; launch.workingDirectory = directory_;
    const auto &definition = nodeDefinition(n.kind);
    (this->*definition.prepare)(id, input, *j, launch);
    auto &program = launch.program; auto &workingDirectory = launch.workingDirectory;
    auto &args = launch.args; auto &standardInput = launch.standardInput;
    QString resolved = resolveExecutable(program);
    if (resolved.isEmpty()) throw std::runtime_error((strings::ExecutableNotFound + program).toStdString());
    // Only explicit shell nodes interpret shell syntax; agent prompts and script paths never do.
    if (resolved.endsWith(strings::CmdExtension, Qt::CaseInsensitive) || resolved.endsWith(strings::BatExtension, Qt::CaseInsensitive))
        throw std::runtime_error(strings::UnsupportedBatchExecutable);
    if (!active_) return;
    auto *process = new QProcess(this); j->process = process;
    process->setWorkingDirectory(workingDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(strings::PWD, workingDirectory);
    for (auto it = launch.environment.cbegin(); it != launch.environment.cend(); ++it) environment.insert(it.key(), it.value());
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, id] {
        if (!jobs_.contains(id)) return;
        auto bytes = jobs_[id]->process->readAllStandardOutput();
        auto &data = jobs_[id]->stdoutData;
        if (data.size() + bytes.size() > limits::ProcessOutputBytes) { fail(id, QString(strings::StdoutTooLarge).arg(limits::ProcessOutputBytes / limits::Mebibyte)); return; }
        data += bytes;
        if (jobs_[id]->eventsOutput) appendLog(id, QString(jobs_[id]->stdoutDecoder.decode(bytes)).toUtf8());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, id] {
        if (jobs_.contains(id)) {
            auto &job = *jobs_[id]; const auto bytes = job.process->readAllStandardError();
            if (job.captureStderr) {
                if (job.stderrData.size() + bytes.size() > limits::ProcessOutputBytes) { fail(id, QString(strings::StderrTooLarge).arg(limits::ProcessOutputBytes / limits::Mebibyte)); return; }
                job.stderrData += bytes;
            }
            appendLog(id, QString(job.stderrDecoder.decode(bytes)).toUtf8());
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && jobs_.contains(id)) fail(id, jobs_[id]->process->errorString());
    });
    connect(process, &QProcess::finished, this, [this, id](int code, QProcess::ExitStatus status) { processFinished(id, code, status); });
    jobs_[id] = std::move(j);
    process->start(resolved, args);
    process->write(standardInput);
    process->closeWriteChannel();
}
void Runner::processFinished(Id id, int code, QProcess::ExitStatus status) {
    if (!active_ || !jobs_.contains(id)) return;
    auto &j = *jobs_.at(id);
    const auto remaining = j.process->readAllStandardOutput();
    if (j.stdoutData.size() + remaining.size() > limits::ProcessOutputBytes) { fail(id, QString(strings::StdoutTooLarge).arg(limits::ProcessOutputBytes / limits::Mebibyte)); return; }
    j.stdoutData += remaining;
    if (j.eventsOutput) appendLog(id, QString(j.stdoutDecoder.decode(remaining)).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    const auto errors = j.process->readAllStandardError();
    if (j.captureStderr) {
        if (j.stderrData.size() + errors.size() > limits::ProcessOutputBytes) { fail(id, QString(strings::StderrTooLarge).arg(limits::ProcessOutputBytes / limits::Mebibyte)); return; }
        j.stderrData += errors;
    }
    appendLog(id, QString(j.stderrDecoder.decode(errors)).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    if ((!j.allowNonzeroExit && code != 0) || status != QProcess::NormalExit) { fail(id, QString(strings::ProcessAbnormalExit).arg(kindName(flow_.nodes.at(id).kind)).arg(code)); return; }
    try {
        auto &n = flow_.nodes.at(id); Json output;
        output = (this->*nodeDefinition(n.kind).decode)(id, j, code);
        j.process->deleteLater(); jobs_.erase(id);
        complete(id, output); schedulePump();
    } catch (const std::exception &e) { fail(id, QString::fromUtf8(e.what())); }
}
}
