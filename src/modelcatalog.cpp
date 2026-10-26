#include "modelcatalog.h"
#include "cli.h"
#include <QDir>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>

namespace an {
namespace {
const QStringList agents{"Codex", "Claude Code", "OpenCode"};
QString text(const Json &value, const char *key) {
    return value.contains(key) && value.at(key).is_string() ? QString::fromStdString(value.at(key).get<std::string>()) : QString();
}
}
ModelCatalog::ModelCatalog(QObject *parent, int timeoutMs) : QObject(parent), timeoutMs_(qMax(1, timeoutMs)) {}
ModelCatalog::~ModelCatalog() {
    for (auto &process : processes_) if (process) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) { process->kill(); process->waitForFinished(1000); }
    }
}
QString ModelCatalog::program(const QString &agent, const AgentConfig &config) const {
    return agent == "Codex" ? config.codex : agent == "Claude Code" ? config.claude : config.opencode;
}
void ModelCatalog::setConfig(const AgentConfig &config) {
    for (auto &agent : agents) if (!configured_ || program(agent, config_) != program(agent, config) ||
        (agent == "OpenCode" && config_.opencodeStandalone != config.opencodeStandalone)) {
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
    if (!force && entry.state == CatalogState::Ready && entry.lastSuccess.secsTo(now) < 600) return;
    if (!force && entry.lastAttempt.isValid() && entry.lastAttempt.msecsTo(now) < 30000) return;
    entry.state = CatalogState::Loading; entry.error.clear(); entry.lastAttempt = now;
    emit changed(agent);
    auto resolved = resolveExecutable(program(agent, config_));
    if (resolved.isEmpty()) { finish(agent, "CLIが見つかりません。ツール → CLI設定を確認してください。"); return; }
    if (resolved.endsWith(".cmd", Qt::CaseInsensitive) || resolved.endsWith(".bat", Qt::CaseInsensitive)) {
        finish(agent, "ネイティブのCLI実行ファイルを指定してください（.cmd/.batは非対応）。"); return;
    }
    QStringList args;
    if (agent == "Codex") args << "app-server";
    else if (agent == "Claude Code") {
        args << "--print" << "--input-format" << "stream-json" << "--output-format" << "stream-json" << "--verbose" << "--no-session-persistence"
             << "--settings" << "{\"disableAllHooks\":true}" << "--strict-mcp-config" << "--mcp-config" << "{\"mcpServers\":{}}";
    }
    else { args << "models"; if (config_.opencodeStandalone) args << "--standalone"; }
    auto job = std::make_unique<Job>();
    auto *process = new QProcess(this); auto *timer = new QTimer(this);
    job->process = process; job->timer = timer; timer->setSingleShot(true);
    processes_.removeIf([](auto &p) { return p.isNull(); }); processes_.push_back(process);
    process->setWorkingDirectory(QDir::currentPath());
    jobs_[agent] = std::move(job);
    connect(process, &QProcess::started, this, [this, agent] {
        if (!jobs_.contains(agent)) return;
        if (agent == "Codex") write(agent, {{"id", 1}, {"method", "initialize"}, {"params", {{"clientInfo", {{"name", "AgentNodify"}, {"version", "0.1.0"}}}}}});
        else if (agent == "Claude Code") write(agent, {{"type", "control_request"}, {"request_id", "catalog-initialize"}, {"request", {{"subtype", "initialize"}}}});
        else jobs_.at(agent)->process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, agent] { read(agent); });
    // Drain diagnostics, but never persist init/account data, credentials, or CLI logs.
    connect(process, &QProcess::readyReadStandardError, this, [process] { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, agent](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && jobs_.contains(agent)) finish(agent, "CLIを起動できません: " + jobs_.at(agent)->process->errorString());
    });
    connect(process, &QProcess::finished, this, [this, agent](int code, QProcess::ExitStatus status) { exited(agent, code, status); });
    connect(timer, &QTimer::timeout, this, [this, agent] { finish(agent, "モデル一覧の取得がタイムアウトしました。"); });
    timer->start(timeoutMs_); process->start(resolved, args);
}
void ModelCatalog::write(const QString &agent, const Json &message) {
    if (jobs_.contains(agent)) jobs_.at(agent)->process->write(QByteArray::fromStdString(message.dump()) + '\n');
}
void ModelCatalog::read(const QString &agent) {
    if (!jobs_.contains(agent)) return;
    auto &job = *jobs_.at(agent);
    auto bytes = job.process->readAllStandardOutput(); job.bytes += bytes.size();
    if (job.bytes > 4 * 1024 * 1024) { finish(agent, "モデル一覧の応答が4MiBを超えました。"); return; }
    job.buffer += bytes;
    if (agent == "OpenCode") return;
    while (jobs_.contains(agent)) {
        auto &buffer = jobs_.at(agent)->buffer; auto end = buffer.indexOf('\n'); if (end < 0) break;
        auto line = buffer.left(end).trimmed(); buffer.remove(0, end + 1);
        if (!line.isEmpty()) handle(agent, line);
    }
}
void ModelCatalog::addModels(Job &job, const Json &models, const QString &agent) {
    if (!models.is_array()) throw std::runtime_error("モデル一覧の応答形式が不正です。");
    for (auto &model : models) {
        if (!model.is_object()) throw std::runtime_error("モデル情報の応答形式が不正です。");
        if (model.value("hidden", false)) continue;
        auto id = agent == "Codex" ? text(model, "model") : text(model, "value");
        if (id.isEmpty() && agent == "Codex") id = text(model, "id");
        if (id.trimmed().isEmpty() || id.size() > 512 || id.contains('\n') || id.contains('\r')) throw std::runtime_error("モデルIDが不正です。");
        auto duplicate = std::find_if(job.models.begin(), job.models.end(), [&](auto &m) { return m.id == id; });
        if (duplicate != job.models.end()) continue;
        auto name = text(model, "displayName"); if (name.isEmpty()) name = id;
        auto description = text(model, "description");
        auto resolved = text(model, "resolvedModel"); if (!resolved.isEmpty()) description += "\n" + resolved;
        job.models.push_back({id, name.left(2000), description.left(4000)});
        if (job.models.size() > 10000) throw std::runtime_error("モデル一覧が10000件を超えました。");
    }
}
void ModelCatalog::handle(const QString &agent, const QByteArray &line) {
    try {
        auto value = Json::parse(line.toStdString()); auto &job = *jobs_.at(agent);
        if (agent == "Codex") {
            // Never service tool/permission requests while discovering metadata.
            if (value.contains("method") && value.contains("id")) {
                write(agent, {{"id", value["id"]}, {"error", {{"code", -32601}, {"message", "Catalog-only client"}}}}); return;
            }
            if (!value.contains("id") || value["id"] != job.requestId) return;
            if (value.contains("error")) throw std::runtime_error("Codexがモデル一覧要求を拒否しました。");
            const auto &result = value.at("result");
            if (!job.initialized) {
                job.initialized = true; write(agent, {{"method", "initialized"}});
                write(agent, {{"id", ++job.requestId}, {"method", "model/list"}, {"params", {{"limit", 100}}}}); return;
            }
            addModels(job, result.at("data"), agent);
            const auto cursor = text(result, "nextCursor");
            if (!cursor.isEmpty()) {
                if (job.seenCursors.contains(cursor) || job.seenCursors.size() >= 100) throw std::runtime_error("モデル一覧のページ送りが終了しません。");
                job.seenCursors.insert(cursor);
                write(agent, {{"id", ++job.requestId}, {"method", "model/list"}, {"params", {{"limit", 100}, {"cursor", cursor.toStdString()}}}}); return;
            }
            finish(agent);
        } else {
            if (value.value("type", "") == "control_request") {
                write(agent, {{"type", "control_response"}, {"response", {{"subtype", "error"}, {"request_id", value.at("request_id")}, {"error", "Catalog-only client"}}}}); return;
            }
            if (value.value("type", "") != "control_response") return;
            auto &response = value.at("response");
            if (response.value("request_id", "") != "catalog-initialize") return;
            if (response.value("subtype", "") != "success") throw std::runtime_error("Claude Codeの初期化に失敗しました。");
            addModels(job, response.at("response").at("models"), agent); finish(agent);
        }
    } catch (const Json::parse_error &) { finish(agent, "CLIのモデル一覧応答が正しいJSONではありません。"); }
    catch (const std::exception &error) { finish(agent, QString::fromUtf8(error.what())); }
}
void ModelCatalog::exited(const QString &agent, int code, QProcess::ExitStatus status) {
    read(agent); if (!jobs_.contains(agent)) return;
    if (code != 0 || status != QProcess::NormalExit) { finish(agent, QString("CLIが異常終了しました（exit %1）。認証とCLI設定を確認してください。").arg(code)); return; }
    if (agent != "OpenCode") {
        auto remaining = jobs_.at(agent)->buffer.trimmed();
        if (!remaining.isEmpty()) handle(agent, remaining);
        if (jobs_.contains(agent)) finish(agent, "モデル一覧を受信する前にCLIが終了しました。");
        return;
    }
    auto output = QString::fromUtf8(jobs_.at(agent)->buffer);
    output.remove(QRegularExpression("\x1b\\[[0-9;]*[A-Za-z]"));
    const QRegularExpression idPattern("^[A-Za-z0-9_.:-]+/[A-Za-z0-9_./:+@-]+$");
    for (auto &line : output.split('\n')) {
        auto id = line.trimmed(); if (id.isEmpty()) continue;
        if (id.size() > 512 || !idPattern.match(id).hasMatch()) { finish(agent, "OpenCodeのモデル一覧の応答形式が不正です。GUI版ではなくCLI版を指定してください。"); return; }
        auto &models = jobs_.at(agent)->models;
        if (std::none_of(models.begin(), models.end(), [&](auto &m) { return m.id == id; })) models.push_back({id, id, {}});
        if (models.size() > 10000) { finish(agent, "モデル一覧が10000件を超えました。"); return; }
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
