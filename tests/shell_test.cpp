#include "shell_test.h"
#include "runner.h"
#include "cli.h"
#include "history.h"
#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

namespace an {
namespace {
bool available() {
#ifdef Q_OS_WIN
    return !resolveExecutable("powershell").isEmpty() || !resolveExecutable("pwsh").isEmpty();
#else
    return QFileInfo::exists("/bin/sh");
#endif
}
QString literal(QString text) {
#ifdef Q_OS_WIN
    text.replace("'", "''");
#else
    text.replace("'", "'\\''");
#endif
    return "'" + text + "'";
}
QString command(const QString &out = {}, const QString &err = {}, int code = 0) {
#ifdef Q_OS_WIN
    return "[Console]::Out.Write(" + literal(out) + ");\n[Console]::Error.Write(" + literal(err) + ");\nexit " + QString::number(code);
#else
    return "printf '%s' " + literal(out) + ";\nprintf '%s' " + literal(err) + " >&2;\nexit " + QString::number(code);
#endif
}
QString cwdCommand() {
#ifdef Q_OS_WIN
    return "[Console]::Out.Write((Get-Location).Path); [Console]::Error.Write($env:PWD)";
#else
    return "printf '%s' \"$(pwd)\"; printf '%s' \"$PWD\" >&2";
#endif
}
Flow shellFlow(const QString &text) {
    auto flow = Flow::initial(); Node node; node.id = 3; node.kind = Kind::Shell; node.title = "シェル"; node.shellCommand = text;
    flow.nodes[3] = node; flow.edges = {{1, 3}, {3, 2}}; return flow;
}
}
void ShellTest::definition() {
    QCOMPARE(kindKey(Kind::Shell), QString("shell")); QCOMPARE(kindName(Kind::Shell), QString("シェル実行")); QVERIFY(parseKind("shell") == Kind::Shell);
    auto flow = shellFlow(command("Hello")); flow.nodes[3].shellDirectory = "work";
    QVERIFY(flow.validate().isEmpty()); QCOMPARE(flow.inputCount(flow.nodes[3]), 1u); QCOMPARE(flow.outputCount(flow.nodes[3]), 1u);
    QVERIFY(Flow::fromJson(flow.toJson()).toJson() == flow.toJson()); QVERIFY(!flow.toJson().contains("sourcePath"));
    flow.nodes[3].shellCommand.clear(); QVERIFY(!flow.validate().isEmpty()); flow.nodes[3].shellCommand = command("${missing}"); QVERIFY(!flow.validate().isEmpty());
    flow.nodes[3].shellCommand = command("Hello"); Node agent; agent.id = 4; agent.prompt = "${exit_code} ${stdout} ${stderr}"; flow.nodes[4] = agent;
    flow.edges = {{1, 3}, {3, 4}, {4, 2}}; QVERIFY(flow.validate().isEmpty()); flow.nodes[4].prompt = "${absent}"; QVERIFY(!flow.validate().isEmpty());
    flow.nodes[4].kind = Kind::Shell; flow.nodes[4].shellCommand = command("${stdout}", "${stderr}"); QVERIFY(flow.validate().isEmpty());
    flow.nodes[4].shellCommand = command("${stdout..bad}"); QVERIFY(!flow.validate().isEmpty());
}
void ShellTest::result_data() {
    QTest::addColumn<QString>("out"); QTest::addColumn<QString>("err"); QTest::addColumn<int>("code");
    QTest::newRow("success UTF8 multiline") << QString("日本語\nstdout '$()' \"x\"") << QString("diagnostic 日本語\n") << 0;
    QTest::newRow("nonzero") << QString("out") << QString("error") << 7;
    QTest::newRow("empty") << QString() << QString() << 0;
    QTest::newRow("JSON stdout remains a string") << QString("{\"a\":1}\n") << QString() << 0;
}
void ShellTest::result() {
    if (!available()) QSKIP("System shell not installed"); QFETCH(QString, out); QFETCH(QString, err); QFETCH(int, code);
    QTemporaryDir directory; const auto flow = shellFlow(command(out, err, code));
    Runner runner; QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 3 && entry.state == State::Succeeded) history.push_back(entry); });
    QVERIFY(runner.start(flow, directory.path(), {}, directory.filePath("flow.json"))); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000);
    QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    const Json expected = {{"exit_code", code}, {"stdout", out.toStdString()}, {"stderr", err.toStdString()}};
    QVERIFY(runner.results().at(2).output == expected); QCOMPARE(history.size(), size_t(1)); QVERIFY(history[0].output == expected);
    QVERIFY(history[0].structuredOutput); QCOMPARE(history[0].executionNumber, quint64(1)); QCOMPARE(history[0].log, err);
    saveNodeExecution(directory.filePath("history"), history[0]); const auto restored = loadNodeExecutions(directory.filePath("history"));
    QCOMPARE(restored.size(), size_t(1)); QVERIFY(restored[0].output == expected); QVERIFY(restored[0].structuredOutput);
}
void ShellTest::directories_data() {
    QTest::addColumn<QString>("mode"); for (QString mode : {"default", "relative", "absolute", "absolute without flow path"}) QTest::newRow(qPrintable(mode)) << mode;
}
void ShellTest::directories() {
    if (!available()) QSKIP("System shell not installed"); QFETCH(QString, mode); QTemporaryDir directory;
    QVERIFY(QDir(directory.path()).mkpath("日本語 flows/work dir")); QVERIFY(QDir(directory.path()).mkpath("project"));
    const auto flowDirectory = directory.filePath("日本語 flows"); auto flow = shellFlow(cwdCommand());
    if (mode == "relative") flow.nodes[3].shellDirectory = "work dir";
    else if (mode.startsWith("absolute")) flow.nodes[3].shellDirectory = directory.filePath("日本語 flows/work dir");
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QString actualDirectory;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 3 && entry.state == State::Succeeded) actualDirectory = entry.directory; });
    QVERIFY(runner.start(flow, directory.filePath("project"), {}, mode == "absolute without flow path" ? QString() : QDir(flowDirectory).filePath("flow.json")));
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    const auto expected = QFileInfo(mode == "default" ? flowDirectory : QDir(flowDirectory).filePath("work dir")).canonicalFilePath();
    QCOMPARE(actualDirectory, expected);
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(runner.results().at(2).output.at("stdout"))), expected);
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(runner.results().at(2).output.at("stderr"))), expected);
}
void ShellTest::savedFlowContext() {
    if (!available()) QSKIP("System shell not installed"); QTemporaryDir directory;
    for (const auto &name : {"old", "new", "project"}) QVERIFY(QDir(directory.path()).mkpath(name));
    const auto path = directory.filePath("old/flow.json"); shellFlow(cwdCommand()).save(path); const auto flow = Flow::load(path);
    QCOMPARE(flow.sourcePath, QFileInfo(path).absoluteFilePath());
    Runner runner; QSignalSpy done(&runner, &Runner::finished);
    QVERIFY(runner.start(flow, directory.filePath("project"), {})); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY(done[0][0].toBool());
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(runner.results().at(2).output.at("stdout"))), QFileInfo(directory.filePath("old")).canonicalFilePath());
    done.clear(); const auto newPath = directory.filePath("new/renamed.json"); flow.save(newPath);
    QVERIFY(runner.start(flow, directory.filePath("project"), {}, newPath)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY(done[0][0].toBool());
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(runner.results().at(2).output.at("stdout"))), QFileInfo(directory.filePath("new")).canonicalFilePath());
}
void ShellTest::variablesAndNonzeroExit() {
    if (!available()) QSKIP("System shell not installed"); QTemporaryDir directory;
    auto flow = shellFlow(command("日本語 value", "diagnostic", 5)); auto next = flow.nodes[3]; next.id = 4; next.shellCommand = command("${stdout}", "${exit_code}: ${stderr}"); flow.nodes[4] = next;
    Node branch; branch.id = 5; branch.kind = Kind::Branch; branch.rules = {{"exit_code", "0", true}}; flow.nodes[5] = branch;
    flow.edges = {{1, 3}, {3, 4}, {4, 5}, {5, 2, 0, 0}, {5, 2, 1, 0}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY2(runner.start(flow, directory.path(), {}, directory.filePath("flow.json")), qPrintable(flow.validate().join('\n')));
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QVERIFY(runner.results().at(2).output == Json({{"exit_code", 0}, {"stdout", "日本語 value"}, {"stderr", "5: diagnostic"}}));
}
void ShellTest::loop() {
    if (!available()) QSKIP("System shell not installed"); QTemporaryDir directory; auto flow = shellFlow(command());
    auto repeat = flow.nodes[3]; repeat.id = 4;
#ifdef Q_OS_WIN
    repeat.shellCommand = "exit (${exit_code} + 1)";
#else
    repeat.shellCommand = "exit $((${exit_code} + 1))";
#endif
    flow.nodes[4] = repeat; Node branch; branch.id = 5; branch.kind = Kind::Branch; branch.rules = {{"exit_code", "3", false}}; flow.nodes[5] = branch;
    flow.edges = {{1, 3}, {3, 4}, {4, 5}, {5, 4, 0, 0}, {5, 2, 1, 0}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 4 && entry.state == State::Succeeded) history.push_back(entry); });
    QVERIFY2(runner.start(flow, directory.path(), {}, directory.filePath("flow.json")), qPrintable(flow.validate().join('\n'))); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000);
    QVERIFY(done[0][0].toBool()); QCOMPARE(history.size(), size_t(3)); QCOMPARE(runner.results().at(4).executions, quint64(3));
    for (size_t i = 0; i < history.size(); ++i) { QCOMPARE(history[i].executionNumber, quint64(i + 1)); QVERIFY(history[i].output.at("exit_code") == i + 1); }
}
void ShellTest::parallelJoin() {
    if (!available()) QSKIP("System shell not installed"); QTemporaryDir directory; auto flow = shellFlow(command("A"));
    auto second = flow.nodes[3]; second.id = 4; second.shellCommand = command("B"); flow.nodes[4] = second;
    Node join; join.id = 5; join.kind = Kind::All; flow.nodes[5] = join;
    auto last = flow.nodes[3]; last.id = 6; last.shellCommand = command("${[0].stdout}:${[1].stdout}"); flow.nodes[6] = last;
    flow.edges = {{1, 3}, {1, 4}, {3, 5, 0, 0}, {4, 5, 0, 1}, {5, 6}, {6, 2}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY(runner.start(flow, directory.path(), {}, directory.filePath("flow.json"))); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000);
    QVERIFY(done[0][0].toBool()); QVERIFY(runner.results().at(2).output.at("stdout") == "A:B");
}
void ShellTest::failures_data() {
    QTest::addColumn<QString>("mode"); for (QString mode : {"no flow path", "bad cwd", "no deadline", "cancel"}) QTest::newRow(qPrintable(mode)) << mode;
}
void ShellTest::failures() {
    if (!available()) QSKIP("System shell not installed"); QFETCH(QString, mode); QTemporaryDir directory; auto flow = shellFlow(command());
    if (mode == "bad cwd") flow.nodes[3].shellDirectory = "absent";
    const bool sleep = mode == "no deadline" || mode == "cancel";
    if (sleep) {
#ifdef Q_OS_WIN
        flow.nodes[3].shellCommand = "Start-Sleep -Seconds 30";
#else
        flow.nodes[3].shellCommand = "exec sleep 30";
#endif
    }
    AgentConfig config; Runner runner; QSignalSpy done(&runner, &Runner::finished);
    QVERIFY(runner.start(flow, directory.path(), config, mode == "no flow path" ? QString() : directory.filePath("flow.json")));
    if (mode == "no deadline") { QTest::qWait(1200); QVERIFY(done.isEmpty()); QVERIFY(runner.running()); QVERIFY(runner.findChildren<QTimer *>().isEmpty()); }
    if (sleep) { QTRY_VERIFY(runner.results().at(3).state == State::Running); runner.cancel(); }
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY(!done[0][0].toBool()); QVERIFY(runner.results().at(2).state != State::Succeeded);
    QVERIFY(runner.results().at(3).state == (sleep ? State::Cancelled : State::Failed));
}
void ShellTest::outputLimits_data() {
    QTest::addColumn<bool>("stderrStream"); QTest::newRow("stdout") << false; QTest::newRow("stderr") << true;
}
void ShellTest::outputLimits() {
    if (!available()) QSKIP("System shell not installed"); QFETCH(bool, stderrStream); QTemporaryDir directory;
#ifdef Q_OS_WIN
    const auto source = QString("[Console]::%1.Write(('x' * (33 * 1024 * 1024)))").arg(stderrStream ? "Error" : "Out");
#else
    const QString source = "exec head -c 34603008 /dev/zero" + QString(stderrStream ? " >&2" : "");
#endif
    Runner runner; QSignalSpy done(&runner, &Runner::finished); AgentConfig config;
    QVERIFY(runner.start(shellFlow(source), directory.path(), config, directory.filePath("flow.json"))); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 25000);
    QVERIFY(!done[0][0].toBool()); QVERIFY(runner.results().at(3).log.contains("32MiB"));
}
void ShellTest::example() {
    if (!available()) QSKIP("System shell not installed"); const QString root = AGENTNODIFY_SOURCE_DIR;
    const auto flow = Flow::load(root + "/examples/shell.flow.json"); Runner runner; QSignalSpy done(&runner, &Runner::finished);
    QVERIFY(runner.start(flow, root, {})); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY(done[0][0].toBool());
    QVERIFY(runner.results().at(2).output.at("exit_code") == 0); QCOMPARE(QString::fromStdString(runner.results().at(2).output.at("stdout")).trimmed(), QString("Hello from shell"));
}
void ShellTest::shellDiagnostics() {
    if (!available()) QSKIP("System shell not installed"); QTemporaryDir directory;
#ifdef Q_OS_WIN
    const QString source = "Write-Error 'diagnostic error'; exit 9";
#else
    const QString source = "printf '%s' 'diagnostic error' >&2; exit 9";
#endif
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY(runner.start(shellFlow(source), directory.path(), {}, directory.filePath("flow.json")));
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY(done[0][0].toBool());
    const auto &output = runner.results().at(2).output; QVERIFY(output.at("exit_code") == 9);
    const auto text = QString::fromStdString(output.at("stderr")); QVERIFY(text.contains("diagnostic error")); QVERIFY(!text.contains("#< CLIXML"));
}
}
QTEST_GUILESS_MAIN(an::ShellTest)
