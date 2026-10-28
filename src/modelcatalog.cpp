#include "constants.h"
#include "strings.h"
#include "modelcatalog.h"
#include "cli.h"
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>

namespace an {
namespace {
const QStringList agents{strings::Codex, strings::ClaudeCode, strings::OpenCode};
QString text(const Json &value, const char *key) {
    return value.contains(key) && value.at(key).is_string() ? QString::fromStdString(value.at(key).get<std::string>()) : QString();
}
}
ModelCatalog::ModelCatalog(QObject *parent, int timeoutMs) : QObject(parent), timeoutMs_(qMax(1, timeoutMs)) {}
ModelCatalog::~ModelCatalog() {
    for (auto &process : processes_) if (process) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) { process->kill(); process->waitForFinished(time::CatalogKillMilliseconds); }
    }
}
QString ModelCatalog::program(const QString &agent, const AgentConfig &config) const {
    return agent == strings::Codex ? config.codex : agent == strings::ClaudeCode ? config.claude : config.opencode;
}
void ModelCatalog::setConfig(const AgentConfig &config) {
    for (auto &agent : agents) if (!configured_ || program(agent, config_) != program(agent, config) ||
        (agent == strings::OpenCode && config_.opencodeStandalone != config.opencodeStandalone)) {
        stop(agent); entries_[agent] = {};
    }
    config_ = config; configured_ = true;
    for (auto &agent : agents) emit changed(agent);
}
CatalogEntry ModelCatalog::entry(const QString &agent) const {
    auto found = entries_.find(agent); return found == entries_.end() ? CatalogEntry{} : found->second;
}
void ModelCatalog::refreshAll(bool force) { for (auto &agent : agents) request(agent, force); }
void ModelCatalog::request(const QString &agent, bool force) {
    if (!configured_ || !agents.contains(agent) || jobs_.contains(agent)) return;
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
    QStringList args;
    if (agent == strings::Codex) args << strings::AppServer;
    else if (agent == strings::ClaudeCode) {
        args << strings::PrintArgument << strings::InputFormatArgument << strings::StreamJson << strings::OutputFormatArgument << strings::StreamJson << strings::VerboseArgument << strings::NoSessionPersistenceArgument
             << strings::SettingsArgument << strings::DisableHooksSettings << strings::StrictMcpArgument << strings::McpConfigArgument << strings::EmptyMcpSettings;
    }
    else { args << strings::Models; if (config_.opencodeStandalone) args << strings::StandaloneArgument; }
    auto job = std::make_unique<Job>();
    auto *process = new QProcess(this); auto *timer = new QTimer(this);
    job->process = process; job->timer = timer; timer->setSingleShot(true);
    processes_.removeIf([](auto &p) { return p.isNull(); }); processes_.push_back(process);
    process->setWorkingDirectory(QDir::currentPath());
    jobs_[agent] = std::move(job);
    connect(process, &QProcess::started, this, [this, agent] {
        if (!jobs_.contains(agent)) return;
        if (agent == strings::Codex) write(agent, {{strings::Id, 1}, {strings::Method, strings::Initialize}, {strings::Params, {{strings::ClientInfo, {{strings::Name, strings::ApplicationName}, {strings::Version, strings::ApplicationVersion}}}}}});
        else if (agent == strings::ClaudeCode) write(agent, {{strings::Type, strings::ControlRequest}, {strings::RequestId, strings::CatalogInitialize}, {strings::Request, {{strings::Subtype, strings::Initialize}}}});
        else jobs_.at(agent)->process->closeWriteChannel();
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
void ModelCatalog::read(const QString &agent) {
    if (!jobs_.contains(agent)) return;
    auto &job = *jobs_.at(agent);
    auto bytes = job.process->readAllStandardOutput(); job.bytes += bytes.size();
    if (job.bytes > limits::CatalogResponseBytes) { finish(agent, QString(strings::CatalogResponseTooLarge).arg(limits::CatalogResponseBytes / limits::Mebibyte)); return; }
    job.buffer += bytes;
    if (agent == strings::OpenCode) return;
    while (jobs_.contains(agent)) {
        auto &buffer = jobs_.at(agent)->buffer; auto end = buffer.indexOf('\n'); if (end < 0) break;
        auto line = buffer.left(end).trimmed(); buffer.remove(0, end + 1);
        if (!line.isEmpty()) handle(agent, line);
    }
}
void ModelCatalog::addModels(Job &job, const Json &models, const QString &agent) {
    if (!models.is_array()) throw std::runtime_error(strings::InvalidCatalogResponse);
    for (auto &model : models) {
        if (!model.is_object()) throw std::runtime_error(strings::InvalidCatalogModel);
        if (model.value(strings::Hidden, false)) continue;
        auto id = agent == strings::Codex ? text(model, strings::Model) : text(model, strings::Value);
        if (id.isEmpty() && agent == strings::Codex) id = text(model, strings::Id);
        if (id.trimmed().isEmpty() || id.size() > limits::ModelIdCharacters || id.contains('\n') || id.contains('\r')) throw std::runtime_error(strings::InvalidModelId);
        auto duplicate = std::find_if(job.models.begin(), job.models.end(), [&](auto &m) { return m.id == id; });
        if (duplicate != job.models.end()) continue;
        auto name = text(model, strings::DisplayName); if (name.isEmpty()) name = id;
        auto description = text(model, strings::Description);
        auto resolved = text(model, strings::ResolvedModel); if (!resolved.isEmpty()) description += strings::Newline + resolved;
        ModelInfo info{id, name.left(limits::ModelNameCharacters), description.left(limits::ModelDescriptionCharacters)};
        const auto key = agent == strings::Codex ? strings::SupportedReasoningEfforts : strings::SupportedEffortLevels;
        if (model.contains(key) && model.at(key).is_array()) for (const auto &effort : model.at(key)) {
            auto level = effort.is_string() ? QString::fromStdString(effort.get<std::string>()) : effort.is_object() ? text(effort, strings::ReasoningEffort) : QString();
            if (level.isEmpty() && effort.is_object()) level = text(effort, strings::Effort);
            if (!level.isEmpty() && level.size() <= limits::EffortCharacters && !info.efforts.contains(level)) info.efforts.append(level);
        }
        job.models.push_back(std::move(info));
        if (job.models.size() > limits::CatalogModels) throw std::runtime_error(QString(strings::TooManyModels).arg(limits::CatalogModels).toStdString());
    }
}
void ModelCatalog::handle(const QString &agent, const QByteArray &line) {
    try {
        auto value = Json::parse(line.toStdString()); auto &job = *jobs_.at(agent);
        if (agent == strings::Codex) {
            // Never service tool/permission requests while discovering metadata.
            if (value.contains(strings::Method) && value.contains(strings::Id)) {
                write(agent, {{strings::Id, value[strings::Id]}, {strings::ErrorValue, {{strings::Code, defaults::RpcMethodNotFound}, {strings::Message, strings::CatalogOnlyClient}}}}); return;
            }
            if (!value.contains(strings::Id) || value[strings::Id] != job.requestId) return;
            if (value.contains(strings::ErrorValue)) throw std::runtime_error(strings::CodexCatalogRejected);
            const auto &result = value.at(strings::Result);
            if (!job.initialized) {
                job.initialized = true; write(agent, {{strings::Method, strings::Initialized}});
                write(agent, {{strings::Id, ++job.requestId}, {strings::Method, strings::ModelList}, {strings::Params, {{strings::Limit, limits::CatalogPageSize}}}}); return;
            }
            addModels(job, result.at(strings::Data), agent);
            const auto cursor = text(result, strings::NextCursor);
            if (!cursor.isEmpty()) {
                if (job.seenCursors.contains(cursor) || job.seenCursors.size() >= limits::CatalogPages) throw std::runtime_error(strings::CatalogPaginationLimit);
                job.seenCursors.insert(cursor);
                write(agent, {{strings::Id, ++job.requestId}, {strings::Method, strings::ModelList}, {strings::Params, {{strings::Limit, limits::CatalogPageSize}, {strings::Cursor, cursor.toStdString()}}}}); return;
            }
            finish(agent);
        } else {
            if (value.value(strings::Type, strings::Empty) == strings::ControlRequest) {
                write(agent, {{strings::Type, strings::ControlResponse}, {strings::Response, {{strings::Subtype, strings::ErrorValue}, {strings::RequestId, value.at(strings::RequestId)}, {strings::ErrorValue, strings::CatalogOnlyClient}}}}); return;
            }
            if (value.value(strings::Type, strings::Empty) != strings::ControlResponse) return;
            auto &response = value.at(strings::Response);
            if (response.value(strings::RequestId, strings::Empty) != strings::CatalogInitialize) return;
            if (response.value(strings::Subtype, strings::Empty) != strings::Success) throw std::runtime_error(strings::ClaudeInitializationFailed);
            addModels(job, response.at(strings::Response).at(strings::Models), agent); finish(agent);
        }
    } catch (const Json::parse_error &) { finish(agent, strings::InvalidCatalogJson); }
    catch (const std::exception &error) { finish(agent, QString::fromUtf8(error.what())); }
}
void ModelCatalog::exited(const QString &agent, int code, QProcess::ExitStatus status) {
    read(agent); if (!jobs_.contains(agent)) return;
    if (code != 0 || status != QProcess::NormalExit) { finish(agent, QString(strings::CatalogAbnormalExit).arg(code)); return; }
    if (agent != strings::OpenCode) {
        auto remaining = jobs_.at(agent)->buffer.trimmed();
        if (!remaining.isEmpty()) handle(agent, remaining);
        if (jobs_.contains(agent)) finish(agent, strings::CatalogEndedEarly);
        return;
    }
    auto output = QString::fromUtf8(jobs_.at(agent)->buffer);
    output.remove(QRegularExpression(strings::AnsiEscapePattern));
    const QRegularExpression idPattern(strings::OpenCodeModelPattern);
    for (auto &line : output.split('\n')) {
        auto id = line.trimmed(); if (id.isEmpty()) continue;
        if (id.size() > limits::ModelIdCharacters || !idPattern.match(id).hasMatch()) { finish(agent, strings::InvalidOpenCodeCatalog); return; }
        auto &models = jobs_.at(agent)->models;
        if (std::none_of(models.begin(), models.end(), [&](auto &m) { return m.id == id; })) models.push_back({id, id, {}});
        if (models.size() > limits::CatalogModels) { finish(agent, QString(strings::TooManyModels).arg(limits::CatalogModels)); return; }
    }
    finish(agent);
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
        entry.models = jobs_.at(agent)->models; entry.state = CatalogState::Ready;
        entry.lastSuccess = QDateTime::currentDateTimeUtc(); entry.error.clear();
    } else { entry.state = CatalogState::Error; entry.error = error; }
    stop(agent); emit changed(agent);
}
}
