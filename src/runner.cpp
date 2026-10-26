#include "runner.h"
#include "cli.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <stdexcept>

namespace an {
QString stateName(State s) {
    switch(s) {
    case State::Pending: return "待機"; case State::Running: return "実行中";
    case State::Succeeded: return "完了"; case State::Skipped: return "スキップ";
    case State::Cancelled: return "中止"; default: return "エラー";
    }
}
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured) {
    QString text, finalText; Json structuredValue; bool hasStructured = false;
    for (auto &line : data.split('\n')) {
        if (line.trimmed().isEmpty()) continue;
        auto j = Json::parse(line.toStdString(), nullptr, false);
        if (j.is_discarded()) throw std::runtime_error("CLIから不正なJSONイベントを受信しました");
        if (j.value("is_error", false) || j.value("type", "") == "error") throw std::runtime_error(j.dump());
        if (agent == "Claude Code") {
            if (j.value("type", "") == "result") {
                if (j.contains("structured_output")) { structuredValue = j["structured_output"]; hasStructured = true; }
                if (j.contains("result") && j["result"].is_string()) finalText = QString::fromStdString(j["result"].get<std::string>());
            } else if (j.value("type", "") == "assistant" && j.contains("message")) {
                for (auto &c : j["message"].value("content", Json::array()))
                    if (c.value("type", "") == "text") text += QString::fromStdString(c.value("text", ""));
            }
        } else if (agent == "OpenCode") {
            if (j.value("type", "") == "text" && j.contains("part")) text += QString::fromStdString(j["part"].value("text", ""));
        } else if (j.value("type", "") == "item.completed" && j.contains("item") && j["item"].value("type", "") == "agent_message") {
            finalText = QString::fromStdString(j["item"].value("text", ""));
        }
    }
    if (structured && hasStructured) return structuredValue;
    if (!finalText.isEmpty()) text = finalText;
    if (text.isEmpty()) throw std::runtime_error("エージェントの最終応答がありません");
    return structured ? parseJson(text.trimmed()) : Json(text.toUtf8().toStdString());
}
Runner::Runner(QObject *parent) : QObject(parent) {}
Runner::~Runner() { active_ = false; stopJobs(); }
bool Runner::start(const Flow &flow, const QString &directory, const AgentConfig &config) {
    if (active_) return false;
    auto errors = flow.validate();
    if (!QFileInfo(directory).isDir()) errors << "プロジェクトディレクトリが存在しません";
    if (!errors.isEmpty()) { emit finished(false, errors.join('\n')); return false; }
    flow_ = flow; directory_ = QFileInfo(directory).canonicalFilePath(); config_ = config;
    results_.clear(); tokens_.clear(); jobs_.clear();
    ++generation_; active_ = true; pumpScheduled_ = false; startPending_ = true;
    for (auto &[id, n] : flow_.nodes) {
        tokens_[id].resize(flow_.inputCount(n)); results_[id] = {}; emit changed(id);
    }
    schedulePump(); return true;
}
void Runner::appendLog(Id id, const QByteArray &bytes) {
    auto &log = results_[id].log;
    log += QString::fromUtf8(bytes);
    constexpr qsizetype limit = 2 * 1024 * 1024;
    if (log.size() > limit) log = "[古いログを省略]\n" + log.right(limit);
    emit changed(id);
}
void Runner::stopJobs() {
    for (auto &[id, j] : jobs_) {
        j->timer->stop(); j->process->disconnect(this);
        if (j->process->state() != QProcess::NotRunning) { j->process->kill(); j->process->waitForFinished(3000); }
        j->timer->deleteLater(); j->process->deleteLater();
    }
    jobs_.clear();
}
void Runner::cancel() {
    if (!active_) return;
    active_ = false; stopJobs();
    for (auto &[id, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; emit changed(id); }
    emit finished(false, "実行を中止しました");
}
void Runner::fail(Id id, const QString &message) {
    if (!active_) return;
    results_[id].state = State::Failed; results_[id].log += "\n" + message; emit changed(id);
    active_ = false; stopJobs();
    for (auto &[other, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; emit changed(other); }
    const auto &node = flow_.nodes.at(id);
    emit finished(false, (node.title.isEmpty() ? kindName(node.kind) : node.title) + ": " + message);
}
void Runner::complete(Id id, const Json &output, int selected) {
    auto &r = results_[id]; r.state = State::Succeeded;
    r.output = output; emit changed(id);
    emit fired(id, r.executions, QString::fromStdString(output.dump()));
    if (!active_) return; // A consumer may cancel from a progress signal.
    for (auto &e : flow_.edges) if (e.from == id) {
        if (selected < 0 || int(e.output) == selected) tokens_[e.to][e.input].push_back(output);
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
    bool progress = false; unsigned budget = 128;
    do {
        progress = false;
        for (auto &[id, n] : flow_.nodes) {
            if (!active_ || budget == 0) break;
            if (jobs_.contains(id)) continue; // Serial per node, FIFO while busy.
            auto &inputs = tokens_.at(id);
            bool ready = n.kind == Kind::Start ? startPending_ : true;
            for (auto &queue : inputs) if (queue.empty()) ready = false;
            if (!ready) continue;
            --budget;
            progress = true;
            Json input;
            if (n.kind == Kind::And) {
                input = Json::array();
                // Pop the complete set BEFORE publishing, including self-loops.
                for (auto &queue : inputs) { input.push_back(std::move(queue.front())); queue.pop_front(); }
            } else if (n.kind == Kind::Start) { input = Json::object(); startPending_ = false; }
            else { input = std::move(inputs.at(0).front()); inputs.at(0).pop_front(); }
            ++results_[id].executions;
            try {
                if (n.kind == Kind::Agent) {
                    results_[id].state = State::Running;
                    appendLog(id, QString("\n--- 実行 %1 ---\n").arg(results_[id].executions).toUtf8());
                    if (active_) launch(id, input);
                }
                else if (n.kind == Kind::Branch) {
                    int selected = int(n.rules.size());
                    for (int i = 0; i < int(n.rules.size()); ++i) {
                        auto &rule = n.rules[size_t(i)];
                        if ((display(resolve(input, rule.variable)) == rule.value) == rule.equals) { selected = i; break; }
                    }
                    complete(id, input, selected);
                } else complete(id, input);
            } catch (const std::exception &e) { fail(id, QString::fromUtf8(e.what())); }
        }
    } while (progress && active_ && budget > 0);
    pumping_ = false;
    if (!active_) return;
    // Even a synchronous infinite loop must yield to cancellation / GUI events.
    if (budget == 0) { schedulePump(); return; }
    if (!jobs_.empty()) return;
    for (auto &[id, inputs] : tokens_) for (auto &queue : inputs) if (!queue.empty()) {
        fail(id, "実行が停止しました（ANDの全入力が揃わず、発火可能なノードがありません）"); return;
    }
    bool reachedEnd = false;
    for (auto &[id, n] : flow_.nodes) if (n.kind == Kind::End && results_[id].executions > 0) reachedEnd = true;
    if (!reachedEnd) {
        for (auto &[id, n] : flow_.nodes) if (n.kind == Kind::End) { fail(id, "終端に到達せず実行が停止しました"); return; }
    }
    for (auto &[id, r] : results_) if (r.executions == 0) { r.state = State::Skipped; emit changed(id); }
    active_ = false;
    emit finished(true, "フローの実行が完了しました（タスクの達成・ファイル作成結果は各ノードの出力を確認してください）");
}
void Runner::launch(Id id, const Json &input) {
    const auto &n = flow_.nodes.at(id);
    QString prompt = interpolate(n.prompt, input);
    auto j = std::make_unique<Job>();
    j->temp = std::make_unique<QTemporaryDir>();
    if (!j->temp->isValid()) throw std::runtime_error("一時ディレクトリを作成できません");
    QString program; QStringList args;
    if (n.agent == "Codex") {
        program = config_.codex;
        j->finalPath = j->temp->filePath("response.txt");
        args << "exec" << "--json" << "--skip-git-repo-check" << "--output-last-message" << j->finalPath;
        args << "--cd" << directory_;
        if (config_.workspaceWrite) args << "--sandbox" << "workspace-write";
        if (!n.schema.trimmed().isEmpty()) {
            auto path = j->temp->filePath("schema.json"); QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(n.schema.toUtf8()) < 0) throw std::runtime_error("スキーマファイルを書き込めません");
            args << "--output-schema" << path;
        }
        if (!n.model.isEmpty()) args << "--model" << n.model;
        if (!n.effort.isEmpty()) args << "-c" << ("model_reasoning_effort=" + QString::fromStdString(Json(n.effort.toStdString()).dump()));
        args << "-";
    } else if (n.agent == "Claude Code") {
        program = config_.claude; args << "-p" << "--verbose" << "--output-format" << "stream-json";
        if (config_.workspaceWrite) args << "--permission-mode" << "acceptEdits";
        if (!n.effort.isEmpty()) args << "--effort" << n.effort;
        if (!n.model.isEmpty()) args << "--model" << n.model;
        if (!n.schema.trimmed().isEmpty()) args << "--json-schema" << n.schema;
    } else {
        // A shared service owns tool execution and can outlive the launcher's cwd.
        // Keep the server and its tools in this project's process tree instead.
        program = config_.opencode; args << "run" << "--format" << "json" << "--standalone";
        if (!n.effort.isEmpty() && n.model.isEmpty()) throw std::runtime_error("OpenCodeの思考量を指定する場合はモデルを選択してください");
        if (!n.model.isEmpty()) args << "--model" << (n.effort.isEmpty() ? n.model : n.model.section('#', 0, 0) + "#" + n.effort);
        // OpenCode CLI has no documented native output-schema flag.
        if (!n.schema.trimmed().isEmpty()) prompt += "\nReturn only JSON (no Markdown) matching this JSON Schema:\n" + n.schema;
        args << "--" << prompt;
    }
    QString resolved = resolveExecutable(program);
    if (resolved.isEmpty()) throw std::runtime_error(("実行ファイルが見つかりません。CLI設定を確認してください: " + program).toStdString());
    // Do not invoke cmd.exe or interpolate prompts into a shell command.
    if (resolved.endsWith(".cmd", Qt::CaseInsensitive) || resolved.endsWith(".bat", Qt::CaseInsensitive))
        throw std::runtime_error(".cmd/.batは非対応です。CLIのネイティブ.exeを指定してください");
    appendLog(id, ("CLI: " + resolved + "\n作業ディレクトリ: " + directory_ + "\n権限で拒否された操作は実行されません。各CLIの権限設定を確認してください。\n").toUtf8());
    if (!active_) return;
    auto *process = new QProcess(this); j->process = process;
    auto *timer = new QTimer(this); j->timer = timer; timer->setSingleShot(true);
    process->setWorkingDirectory(directory_);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PWD", directory_);
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, id] {
        if (!jobs_.contains(id)) return;
        auto bytes = jobs_[id]->process->readAllStandardOutput();
        auto &data = jobs_[id]->stdoutData;
        if (data.size() + bytes.size() > 32 * 1024 * 1024) { fail(id, "CLI出力が32MiBを超えました"); return; }
        data += bytes; appendLog(id, QString(jobs_[id]->stdoutDecoder.decode(bytes)).toUtf8());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, id] {
        if (jobs_.contains(id)) {
            auto &job = *jobs_[id]; appendLog(id, QString(job.stderrDecoder.decode(job.process->readAllStandardError())).toUtf8());
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && jobs_.contains(id)) fail(id, jobs_[id]->process->errorString());
    });
    connect(process, &QProcess::finished, this, [this, id](int code, QProcess::ExitStatus status) { processFinished(id, code, status); });
    connect(timer, &QTimer::timeout, this, [this, id] { fail(id, "エージェントの実行がタイムアウトしました"); });
    jobs_[id] = std::move(j);
    timer->start(qMax(1, config_.timeoutSeconds) * 1000);
    process->start(resolved, args);
    if (n.agent != "OpenCode") process->write(prompt.toUtf8());
    process->closeWriteChannel();
}
void Runner::processFinished(Id id, int code, QProcess::ExitStatus status) {
    if (!active_ || !jobs_.contains(id)) return;
    auto &j = *jobs_.at(id);
    j.timer->stop();
    const auto remaining = j.process->readAllStandardOutput();
    if (j.stdoutData.size() + remaining.size() > 32 * 1024 * 1024) { fail(id, "CLI出力が32MiBを超えました"); return; }
    j.stdoutData += remaining; appendLog(id, QString(j.stdoutDecoder.decode(remaining)).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    appendLog(id, QString(j.stderrDecoder.decode(j.process->readAllStandardError())).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    if (code != 0 || status != QProcess::NormalExit) { fail(id, QString("CLIが異常終了しました (exit %1)").arg(code)); return; }
    try {
        auto &n = flow_.nodes.at(id); Json output;
        if (n.agent == "Codex" && QFileInfo::exists(j.finalPath)) {
            QFile file(j.finalPath); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("最終応答ファイルを開けません");
            auto data = file.readAll(); output = n.schema.trimmed().isEmpty() ? Json(data.toStdString()) : Json::parse(data.toStdString());
        } else output = decodeAgentOutput(n.agent, j.stdoutData, !n.schema.trimmed().isEmpty());
        if (!n.schema.trimmed().isEmpty()) validateOutput(parseJson(n.schema), output);
        j.timer->deleteLater(); j.process->deleteLater(); jobs_.erase(id);
        complete(id, output); schedulePump();
    } catch (const std::exception &e) { fail(id, QString::fromUtf8(e.what())); }
}
}
