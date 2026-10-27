#include "runner.h"
#include <QPointer>
#include "cli.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QUuid>
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
bool Runner::start(const Flow &flow, const QString &directory, const AgentConfig &config, const QString &flowPath) {
    if (active_) return false;
    auto errors = flow.validate();
    if (!QFileInfo(directory).isDir()) errors << "プロジェクトディレクトリが存在しません";
    if (!errors.isEmpty()) { emit finished(false, errors.join('\n')); return false; }
    flow_ = flow; directory_ = QFileInfo(directory).canonicalFilePath(); config_ = config;
    const auto source = flowPath.isEmpty() ? flow.sourcePath : flowPath;
    flowDirectory_ = source.isEmpty() ? QString() : QFileInfo(source).absolutePath();
    results_.clear(); tokens_.clear(); jobs_.clear(); activeExecutions_.clear();
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
    if (flow_.nodes.at(id).kind == Kind::Agent) execution.agent = flow_.nodes.at(id).agent;
    if (flow_.nodes.at(id).kind == Kind::Script) execution.agent = flow_.nodes.at(id).scriptLanguage == "python" ? "Python" : "Node.js";
    if (flow_.nodes.at(id).kind == Kind::Shell) {
#ifdef Q_OS_WIN
        execution.agent = "PowerShell";
#else
        execution.agent = "sh";
#endif
    }
    execution.structuredOutput = isHostOperation(flow_.nodes.at(id).kind) || flow_.nodes.at(id).kind == Kind::Shell || (flow_.nodes.at(id).kind == Kind::Agent && !flow_.nodes.at(id).schema.trimmed().isEmpty());
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
    pendingHost_.clear();
    if (cancelHostOperations) cancelHostOperations(this);
    for (auto &[id, j] : jobs_) {
        j->process->disconnect(this);
        if (j->process->state() != QProcess::NotRunning) { j->process->kill(); j->process->waitForFinished(3000); }
        auto &log = results_[id].log;
        const auto remaining = j->process->readAllStandardOutput();
        if (!j->script && !j->shell) log += QString(j->stdoutDecoder.decode(remaining));
        log += QString(j->stderrDecoder.decode(j->process->readAllStandardError()));
        constexpr qsizetype limit = 2 * 1024 * 1024;
        if (log.size() > limit) log = "[古いログを省略]\n" + log.right(limit);
        j->process->deleteLater();
    }
    jobs_.clear();
}
void Runner::cancel() {
    if (!active_) return;
    active_ = false; stopJobs();
    for (auto &[id, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; finishExecution(id); emit changed(id); }
    emit finished(false, "実行を中止しました");
}
void Runner::fail(Id id, const QString &message) {
    if (!active_) return;
    active_ = false; results_[id].state = State::Failed; stopJobs();
    results_[id].log += "\n" + message; finishExecution(id); emit changed(id);
    for (auto &[other, r] : results_) if (r.state == State::Running || r.state == State::Pending) { r.state = State::Cancelled; finishExecution(other); emit changed(other); }
    const auto &node = flow_.nodes.at(id);
    emit finished(false, (node.title.isEmpty() ? kindName(node.kind) : node.title) + ": " + message);
}
void Runner::complete(Id id, const Json &output, int selected) {
    auto &r = results_[id]; r.state = State::Succeeded;
    r.output = output; finishExecution(id); emit changed(id);
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
            if (jobs_.contains(id) || pendingHost_.contains(id)) continue; // Serial per node, FIFO while busy.
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
            beginExecution(id);
            if (!active_) break;
            try {
                if (n.kind == Kind::Agent || n.kind == Kind::Script || n.kind == Kind::Shell) {
                    if (active_) launch(id, input);
                }
                else if (isHostOperation(n.kind)) {
                    if (!hostHandler) throw std::runtime_error("host_unavailable");
                    const auto path = interpolate(n.projectPath, input);
                    pendingHost_.insert(id);
                    const auto generation = generation_; QPointer<Runner> guard(this);
                    hostHandler(this, id, n, path.trimmed().isEmpty() ? QString() : QDir(directory_).absoluteFilePath(path), [guard, generation, id](const Json &output, const QString &error) {
                        if (!guard) return;
                        QTimer::singleShot(0, guard, [guard, generation, id, output, error] {
                            if (!guard || !guard->active_ || guard->generation_ != generation || !guard->pendingHost_.erase(id)) return;
                            if (!error.isEmpty()) guard->fail(id, error);
                            else { guard->complete(id, output); guard->schedulePump(); }
                        });
                    });
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
    if (!jobs_.empty() || !pendingHost_.empty()) return;
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
    auto j = std::make_unique<Job>();
    QString program, workingDirectory = directory_; QStringList args; QByteArray standardInput;
    if (n.kind == Kind::Shell) {
        j->shell = true;
        if ((n.shellDirectory.trimmed().isEmpty() || QDir::isRelativePath(n.shellDirectory)) && flowDirectory_.isEmpty())
            throw std::runtime_error("シェル実行の既定／相対ディレクトリを解決できません。フローの保存先を指定してください");
        const QFileInfo work(n.shellDirectory.trimmed().isEmpty() ? flowDirectory_ : QDir(flowDirectory_).absoluteFilePath(n.shellDirectory));
        if (!work.isDir()) throw std::runtime_error(("シェルの実行ディレクトリが存在しません: " + work.absoluteFilePath()).toStdString());
        workingDirectory = work.canonicalFilePath();
        const auto command = interpolate(n.shellCommand, input);
#ifdef Q_OS_WIN
        program = resolveExecutable("powershell"); if (program.isEmpty()) program = resolveExecutable("pwsh");
        if (program.isEmpty()) throw std::runtime_error("PowerShellが見つかりません");
        const auto source = "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false); $OutputEncoding = [Console]::OutputEncoding; $ProgressPreference = 'SilentlyContinue';\n" + command;
        args << "-NoLogo" << "-NoProfile" << "-NonInteractive" << "-OutputFormat" << "Text" << "-Command" << source;
#else
        program = "/bin/sh"; args << "-c" << command;
#endif
        activeExecutions_.at(id).directory = workingDirectory; emit executionChanged(activeExecutions_.at(id));
        if (!active_) return;
    } else if (n.kind == Kind::Script) {
        j->script = true;
        const QFileInfo script(QDir(directory_).absoluteFilePath(n.scriptPath));
        if (!script.isFile() || !script.isReadable()) throw std::runtime_error(("スクリプトファイルを読み取れません: " + script.absoluteFilePath()).toStdString());
        const QFileInfo work(n.scriptDirectory.trimmed().isEmpty() ? script.canonicalPath() : QDir(directory_).absoluteFilePath(n.scriptDirectory));
        if (!work.isDir()) throw std::runtime_error(("スクリプトの実行ディレクトリが存在しません: " + work.absoluteFilePath()).toStdString());
        workingDirectory = work.canonicalFilePath();
        activeExecutions_.at(id).directory = workingDirectory; emit executionChanged(activeExecutions_.at(id));
        if (!active_) return;
        if (n.scriptLanguage == "python") {
#ifdef Q_OS_WIN
            program = resolveExecutable("python"); if (program.isEmpty()) program = resolveExecutable("python3");
#else
            program = resolveExecutable("python3"); if (program.isEmpty()) program = resolveExecutable("python");
#endif
            if (program.isEmpty()) throw std::runtime_error("システムのPythonが見つかりません。PythonをインストールしPATHを設定してください");
            args << "-u" << script.canonicalFilePath();
        } else {
            program = resolveExecutable("node");
            if (program.isEmpty()) throw std::runtime_error("システムのNode.jsが見つかりません。Node.jsをインストールしPATHを設定してください");
            args << script.canonicalFilePath();
        }
        standardInput = QByteArray::fromStdString(input.dump()) + '\n';
    } else {
        QString prompt = interpolate(n.prompt, input);
        j->temp = std::make_unique<QTemporaryDir>();
        if (!j->temp->isValid()) throw std::runtime_error("一時ディレクトリを作成できません");
        if (n.agent == "Codex") {
            program = config_.codex;
            j->finalPath = j->temp->filePath("response.txt");
            args << "exec" << "--json" << "--skip-git-repo-check" << "--output-last-message" << j->finalPath;
            args << "--cd" << directory_;
            args << "--sandbox" << "workspace-write";
            if (!n.schema.trimmed().isEmpty()) {
                auto path = j->temp->filePath("schema.json"); QFile file(path);
                if (!file.open(QIODevice::WriteOnly) || file.write(n.schema.toUtf8()) < 0) throw std::runtime_error("スキーマファイルを書き込めません");
                args << "--output-schema" << path;
            }
            if (!n.model.isEmpty()) args << "--model" << n.model;
            if (!n.effort.isEmpty()) args << "-c" << ("model_reasoning_effort=" + QString::fromStdString(Json(n.effort.toStdString()).dump()));
            args << "-";
        } else if (n.agent == "Claude Code") {
            program = config_.claude; args << "-p" << "--verbose" << "--output-format" << "stream-json" << "--include-partial-messages";
            args << "--permission-mode" << "acceptEdits";
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
        if (n.agent != "OpenCode") standardInput = prompt.toUtf8();
    }
    QString resolved = resolveExecutable(program);
    if (resolved.isEmpty()) throw std::runtime_error(("実行ファイルが見つかりません。CLI設定を確認してください: " + program).toStdString());
    // Only explicit shell nodes interpret shell syntax; agent prompts and script paths never do.
    if (resolved.endsWith(".cmd", Qt::CaseInsensitive) || resolved.endsWith(".bat", Qt::CaseInsensitive))
        throw std::runtime_error(".cmd/.batは非対応です。CLIのネイティブ.exeを指定してください");
    if (!active_) return;
    auto *process = new QProcess(this); j->process = process;
    process->setWorkingDirectory(workingDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PWD", workingDirectory);
    if (j->script && n.scriptLanguage == "python") { environment.insert("PYTHONUTF8", "1"); environment.insert("PYTHONIOENCODING", "utf-8"); }
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, id] {
        if (!jobs_.contains(id)) return;
        auto bytes = jobs_[id]->process->readAllStandardOutput();
        auto &data = jobs_[id]->stdoutData;
        if (data.size() + bytes.size() > 32 * 1024 * 1024) { fail(id, "標準出力が32MiBを超えました"); return; }
        data += bytes;
        if (!jobs_[id]->script && !jobs_[id]->shell) appendLog(id, QString(jobs_[id]->stdoutDecoder.decode(bytes)).toUtf8());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, id] {
        if (jobs_.contains(id)) {
            auto &job = *jobs_[id]; const auto bytes = job.process->readAllStandardError();
            if (job.shell) {
                if (job.stderrData.size() + bytes.size() > 32 * 1024 * 1024) { fail(id, "標準エラーが32MiBを超えました"); return; }
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
    if (j.stdoutData.size() + remaining.size() > 32 * 1024 * 1024) { fail(id, "標準出力が32MiBを超えました"); return; }
    j.stdoutData += remaining;
    if (!j.script && !j.shell) appendLog(id, QString(j.stdoutDecoder.decode(remaining)).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    const auto errors = j.process->readAllStandardError();
    if (j.shell) {
        if (j.stderrData.size() + errors.size() > 32 * 1024 * 1024) { fail(id, "標準エラーが32MiBを超えました"); return; }
        j.stderrData += errors;
    }
    appendLog(id, QString(j.stderrDecoder.decode(errors)).toUtf8());
    if (!active_ || !jobs_.contains(id)) return;
    if ((!j.shell && code != 0) || status != QProcess::NormalExit) { fail(id, QString("%1が異常終了しました (exit %2)").arg(kindName(flow_.nodes.at(id).kind)).arg(code)); return; }
    try {
        auto &n = flow_.nodes.at(id); Json output;
        if (j.shell) {
            output = {{"exit_code", code}, {"stdout", QString::fromUtf8(j.stdoutData).toStdString()}, {"stderr", QString::fromUtf8(j.stderrData).toStdString()}};
        } else if (j.script) {
            QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless); const QString text = decoder.decode(j.stdoutData);
            if (decoder.hasError()) throw std::runtime_error("スクリプトの標準出力はUTF-8で出力してください");
            output = Json::parse(j.stdoutData.toStdString(), nullptr, false);
            const bool structured = !output.is_discarded();
            if (!structured) output = text.toUtf8().toStdString();
            activeExecutions_.at(id).structuredOutput = structured;
        } else if (n.agent == "Codex" && QFileInfo::exists(j.finalPath)) {
            QFile file(j.finalPath); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("最終応答ファイルを開けません");
            auto data = file.readAll(); output = n.schema.trimmed().isEmpty() ? Json(data.toStdString()) : Json::parse(data.toStdString());
        } else output = decodeAgentOutput(n.agent, j.stdoutData, !n.schema.trimmed().isEmpty());
        if (!j.script && !j.shell && !n.schema.trimmed().isEmpty()) validateOutput(parseJson(n.schema), output);
        j.process->deleteLater(); jobs_.erase(id);
        complete(id, output); schedulePump();
    } catch (const std::exception &e) { fail(id, QString::fromUtf8(e.what())); }
}
}
