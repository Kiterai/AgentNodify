#include "shared/constants.h"
#include "shared/strings.h"
#include "modelcatalog.h"
#include "cli.h"
#include <QDir>
#include <stdexcept>

namespace an {
ModelCatalog::ModelCatalog(QObject *parent, int timeoutMs) : QObject(parent), timeoutMs_(qMax(1, timeoutMs)) {}
ModelCatalog::~ModelCatalog() {
    for (auto &process : processes_) if (process) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) { process->kill(); process->waitForFinished(time::CatalogKillMilliseconds); }
    }
}
QString ModelCatalog::program(const QString &agent, const AgentConfig &config) const {
    return agentExecutable(agentDefinition(agent),config);
}
void ModelCatalog::setConfig(const AgentConfig &config) {
    for (const auto &definition : agentDefinitions()) {
        const QString agent=definition.name;
        if (!configured_ || program(agent, config_) != program(agent, config) || definition.catalog()->arguments(config_) != definition.catalog()->arguments(config)) {
            stop(agent); entries_[agent] = {};
        }
    }
    config_ = config; configured_ = true;
    for (const auto &agent : agentNames()) emit changed(agent);
}
CatalogEntry ModelCatalog::entry(const QString &agent) const {
    auto found = entries_.find(agent); return found == entries_.end() ? CatalogEntry{} : found->second;
}
void ModelCatalog::refreshAll(bool force) { for (const auto &agent : agentNames()) request(agent, force); }
void ModelCatalog::request(const QString &agent, bool force) {
    if (!configured_ || !findAgentDefinition(agent) || jobs_.contains(agent)) return;
    auto &entry = entries_[agent]; const auto now = QDateTime::currentDateTimeUtc();
    if (!force && entry.state == CatalogState::Ready && entry.lastSuccess.secsTo(now) < limits::CatalogCacheSeconds) return;
    if (!force && entry.lastAttempt.isValid() && entry.lastAttempt.msecsTo(now) < limits::CatalogRetryMilliseconds) return;
    entry.state = CatalogState::Loading; entry.error.clear(); entry.lastAttempt = now;
    emit changed(agent);
    auto resolved = resolveExecutable(program(agent, config_));
    if (resolved.isEmpty()) { finish(agent, strings::CatalogCliNotFound); return; }
    if (resolved.endsWith(strings::CmdExtension, Qt::CaseInsensitive) || resolved.endsWith(strings::BatExtension, Qt::CaseInsensitive)) {
        finish(agent, strings::NativeCliRequired); return;
    }
    auto job = std::make_unique<Job>();
    job->protocol=agentDefinition(agent).catalog();
    const auto args=job->protocol->arguments(config_);
    auto *process = new QProcess(this); auto *timer = new QTimer(this);
    job->process = process; job->timer = timer; timer->setSingleShot(true);
    processes_.removeIf([](auto &p) { return p.isNull(); }); processes_.push_back(process);
    process->setWorkingDirectory(QDir::currentPath());
    jobs_[agent] = std::move(job);
    connect(process, &QProcess::started, this, [this, agent] {
        if (!jobs_.contains(agent)) return;
        apply(agent,jobs_.at(agent)->protocol->started());
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, agent] { read(agent); });
    // Drain diagnostics, but never persist init/account data, credentials, or CLI logs.
    connect(process, &QProcess::readyReadStandardError, this, [process] { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, agent](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && jobs_.contains(agent)) finish(agent, strings::CatalogStartFailed + jobs_.at(agent)->process->errorString());
    });
    connect(process, &QProcess::finished, this, [this, agent](int code, QProcess::ExitStatus status) { exited(agent, code, status); });
    connect(timer, &QTimer::timeout, this, [this, agent] { finish(agent, strings::CatalogTimeout); });
    timer->start(timeoutMs_); process->start(resolved, args);
}
void ModelCatalog::write(const QString &agent, const Json &message) {
    if (jobs_.contains(agent)) jobs_.at(agent)->process->write(QByteArray::fromStdString(message.dump()) + '\n');
}
void ModelCatalog::apply(const QString &agent, const CatalogAction &action) {
    for (const auto &message : action.messages) write(agent,message);
    if (action.closeInput && jobs_.contains(agent)) jobs_.at(agent)->process->closeWriteChannel();
    if (action.finished) finish(agent);
}
void ModelCatalog::read(const QString &agent) {
    if (!jobs_.contains(agent)) return;
    auto &job = *jobs_.at(agent);
    auto bytes = job.process->readAllStandardOutput(); job.bytes += bytes.size();
    if (job.bytes > limits::CatalogResponseBytes) { finish(agent, QString(strings::CatalogResponseTooLarge).arg(limits::CatalogResponseBytes / limits::Mebibyte)); return; }
    job.buffer += bytes;
    if (!job.protocol->lineBased()) return;
    while (jobs_.contains(agent)) {
        auto &buffer = jobs_.at(agent)->buffer; auto end = buffer.indexOf('\n'); if (end < 0) break;
        auto line = buffer.left(end).trimmed(); buffer.remove(0, end + 1);
        if (!line.isEmpty()) handle(agent, line);
    }
}
void ModelCatalog::handle(const QString &agent, const QByteArray &line) {
    try {
        const auto value=Json::parse(line.toStdString());
        const auto action=jobs_.at(agent)->protocol->receive(value);
        apply(agent,action);
    } catch (const Json::parse_error &) { finish(agent, strings::InvalidCatalogJson); }
    catch (const std::exception &error) { finish(agent, QString::fromUtf8(error.what())); }
}
void ModelCatalog::exited(const QString &agent, int code, QProcess::ExitStatus status) {
    read(agent); if (!jobs_.contains(agent)) return;
    if (code != 0 || status != QProcess::NormalExit) { finish(agent, QString(strings::CatalogAbnormalExit).arg(code)); return; }
    if (jobs_.at(agent)->protocol->lineBased()) {
        auto remaining = jobs_.at(agent)->buffer.trimmed();
        if (!remaining.isEmpty()) handle(agent, remaining);
        if (jobs_.contains(agent)) finish(agent, strings::CatalogEndedEarly);
        return;
    }
    try {
        jobs_.at(agent)->protocol->complete(jobs_.at(agent)->buffer);
        finish(agent);
    } catch (const std::exception &error) {finish(agent,QString::fromUtf8(error.what()));}
}
void ModelCatalog::stop(const QString &agent) {
    if (!jobs_.contains(agent)) return;
    auto &job = *jobs_.at(agent); job.timer->stop(); job.timer->deleteLater(); job.process->disconnect(this);
    auto *process = job.process;
    if (process->state() != QProcess::NotRunning) {
        connect(process, &QProcess::finished, process, &QObject::deleteLater); process->kill();
    } else process->deleteLater();
    jobs_.erase(agent);
}
void ModelCatalog::finish(const QString &agent, const QString &error) {
    auto &entry = entries_[agent];
    if (error.isEmpty() && jobs_.contains(agent)) {
        entry.models = jobs_.at(agent)->protocol->models(); entry.state = CatalogState::Ready;
        entry.lastSuccess = QDateTime::currentDateTimeUtc(); entry.error.clear();
    } else { entry.state = CatalogState::Error; entry.error = error; }
    stop(agent); emit changed(agent);
}
}
