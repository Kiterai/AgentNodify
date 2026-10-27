#include "script_test.h"
#include "runner.h"
#include "cli.h"
#include "history.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

namespace an {
namespace {
bool available(const QString &language) {
    return language == "python" ? !resolveExecutable("python").isEmpty() || !resolveExecutable("python3").isEmpty() : !resolveExecutable("node").isEmpty();
}
QString scriptFile(const QString &directory, const QString &name, const QString &source) {
    const auto path = QDir(directory).filePath(name);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return {};
    QFile file(path); const auto bytes = source.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return {};
    return path;
}
Flow scriptFlow(const QString &language, const QString &path) {
    auto flow = Flow::initial(); Node script; script.id = 3; script.kind = Kind::Script; script.title = "変換";
    script.scriptLanguage = language; script.scriptPath = path; flow.nodes[3] = script; flow.edges = {{1, 3}, {3, 2}}; return flow;
}
QString outputSource(const QString &language, const QString &text) {
    const auto literal = QString::fromStdString(Json(text.toStdString()).dump());
    return language == "python" ? "import sys\nsys.stdout.buffer.write(" + literal + ".encode('utf-8'))\n" : "process.stdout.write(" + literal + ");\n";
}
QString extension(const QString &language) { return language == "python" ? ".py" : ".js"; }
}
void ScriptTest::definitionAndValidation() {
    QCOMPARE(kindName(Kind::Script), QString("スクリプト")); QCOMPARE(kindKey(Kind::Script), QString("script")); QVERIFY(parseKind("script") == Kind::Script);
    auto flow = scriptFlow("javascript", "日本語/transform.js"); flow.nodes[3].scriptDirectory = "work dir";
    QVERIFY(flow.validate().isEmpty()); QCOMPARE(flow.inputCount(flow.nodes[3]), 1u); QCOMPARE(flow.outputCount(flow.nodes[3]), 1u);
    const auto restored = Flow::fromJson(flow.toJson()); QVERIFY(restored.toJson() == flow.toJson());
    QCOMPARE(restored.nodes.at(3).scriptPath, flow.nodes.at(3).scriptPath); QCOMPARE(restored.nodes.at(3).scriptDirectory, QString("work dir"));
    flow.nodes[3].scriptLanguage = "ruby"; QVERIFY(!flow.validate().isEmpty()); flow.nodes[3].scriptLanguage = "python";
    flow.nodes[3].scriptPath.clear(); QVERIFY(!flow.validate().isEmpty()); flow.nodes[3].scriptPath = "transform.py";
    Node agent; agent.id = 4; agent.prompt = "${value.nested[0]}"; flow.nodes[4] = agent; flow.edges = {{1, 3}, {3, 4}, {4, 2}};
    QVERIFY2(flow.validate().isEmpty(), qPrintable(flow.validate().join('\n')));
    flow.nodes[4].prompt = "${value..bad}"; QVERIFY(!flow.validate().isEmpty());
    auto legacy = Flow::initial().toJson(); for (auto &node : legacy["nodes"]) { node.erase("scriptLanguage"); node.erase("scriptPath"); node.erase("scriptDirectory"); }
    QCOMPARE(Flow::fromJson(legacy).nodes.at(1).scriptLanguage, QString("python"));
}
void ScriptTest::execution_data() {
    QTest::addColumn<QString>("language"); QTest::addColumn<bool>("explicitDirectory");
    for (const QString language : {"python", "javascript"}) for (bool explicitDir : {false, true})
        QTest::newRow(qPrintable(language + (explicitDir ? " explicit cwd" : " default cwd"))) << language << explicitDir;
}
void ScriptTest::execution() {
    QFETCH(QString, language); QFETCH(bool, explicitDirectory); if (!available(language)) QSKIP("System runtime not installed");
    QTemporaryDir project; QVERIFY(project.isValid()); QVERIFY(QDir(project.path()).mkpath("work dir"));
    const Json value = {{"text", "日本語\nquotes '\" $() ${not_interpolated}"}, {"array", Json::array({1, true, nullptr})}};
    const auto source = scriptFile(project.path(), "日本語 scripts/producer" + extension(language), outputSource(language, QString::fromStdString(value.dump()) + '\n')); QVERIFY(!source.isEmpty());
    const QString transform = language == "python" ?
        "import sys, json, os\nv = json.load(sys.stdin)\nprint(json.dumps({'input': v, 'cwd': os.getcwd(), 'pwd': os.environ['PWD']}, ensure_ascii=False))\nprint('進行 日本語', file=sys.stderr)\n" :
        "const v = JSON.parse(require('node:fs').readFileSync(0, 'utf8'));\nconsole.log(JSON.stringify({input:v, cwd:process.cwd(), pwd:process.env.PWD}));\nprocess.stderr.write('進行 日本語\\n');\n";
    const auto file = scriptFile(project.path(), "日本語 scripts/変換's file" + extension(language), transform); QVERIFY(!file.isEmpty());
    auto flow = scriptFlow(language, QDir(project.path()).relativeFilePath(source));
    auto node = flow.nodes[3]; node.id = 4; node.scriptPath = QDir(project.path()).relativeFilePath(file); node.scriptDirectory = explicitDirectory ? "work dir" : "";
    flow.nodes[4] = node; flow.edges = {{1, 3}, {3, 4}, {4, 2}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 4 && entry.state == State::Succeeded) history.push_back(entry); });
    QVERIFY(runner.start(flow, project.path(), {})); QTRY_COMPARE(done.count(), 1); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    const auto &output = runner.results().at(2).output; QVERIFY(output.at("input") == value);
    const auto directory = QFileInfo(explicitDirectory ? project.filePath("work dir") : QFileInfo(file).absolutePath()).canonicalFilePath();
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(output.at("cwd"))), directory);
    QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(output.at("pwd"))), directory);
    QCOMPARE(history.size(), size_t(1)); QCOMPARE(history[0].directory, directory); QCOMPARE(history[0].executionNumber, quint64(1));
    QCOMPARE(history[0].agent, language == "python" ? QString("Python") : QString("Node.js")); QVERIFY(history[0].structuredOutput);
    QVERIFY(history[0].log.contains("進行 日本語")); QVERIFY(!history[0].log.contains("not_interpolated"));
    saveNodeExecution(project.filePath("history"), history[0]); const auto loaded = loadNodeExecutions(project.filePath("history"));
    QCOMPARE(loaded.size(), size_t(1)); QCOMPARE(loaded[0].directory, directory); QVERIFY(loaded[0].output == output);
}
void ScriptTest::outputTypes_data() {
    QTest::addColumn<QString>("language"); QTest::addColumn<QString>("text"); QTest::addColumn<QString>("expected"); QTest::addColumn<bool>("structured");
    const QStringList texts = {"", "plain\n日本語\n", "\"JSON string\"\n", "42\n", "[1,true,null]\n", "null\n"};
    for (const QString language : {"python", "javascript"}) for (int i = 0; i < texts.size(); ++i) {
        const auto parsed = Json::parse(texts[i].toStdString(), nullptr, false); const bool structured = !parsed.is_discarded();
        const Json expected = structured ? parsed : Json(texts[i].toStdString());
        QTest::newRow(qPrintable(language + QString::number(i))) << language << texts[i] << QString::fromStdString(expected.dump()) << structured;
    }
}
void ScriptTest::outputTypes() {
    QFETCH(QString, language); QFETCH(QString, text); QFETCH(QString, expected); QFETCH(bool, structured); if (!available(language)) QSKIP("System runtime not installed");
    QTemporaryDir project; const auto file = scriptFile(project.path(), "output" + extension(language), outputSource(language, text)); QVERIFY(!file.isEmpty());
    Runner runner; QSignalSpy done(&runner, &Runner::finished); bool savedStructured = !structured;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 3 && entry.state == State::Succeeded) savedStructured = entry.structuredOutput; });
    QVERIFY(runner.start(scriptFlow(language, file), project.path(), {})); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QVERIFY(runner.results().at(2).output == parseJson(expected)); QCOMPARE(savedStructured, structured); QVERIFY(runner.results().at(3).log.isEmpty());
}
void ScriptTest::failures_data() {
    QTest::addColumn<QString>("language"); QTest::addColumn<QString>("mode");
    for (const QString language : {"python", "javascript"}) for (const QString mode : {"missing file", "missing directory", "exit", "no deadline", "cancel", "invalid utf8"})
        QTest::newRow(qPrintable(language + " " + mode)) << language << mode;
}
void ScriptTest::failures() {
    QFETCH(QString, language); QFETCH(QString, mode); if (!available(language)) QSKIP("System runtime not installed");
    QTemporaryDir project;
    const bool sleep = mode == "no deadline" || mode == "cancel";
    QString source = language == "python" ? (sleep ? "import time\ntime.sleep(30)\n" : "import sys\nprint('diagnostic 日本語', file=sys.stderr)\nsys.exit(7)\n") :
        (sleep ? "setTimeout(() => {}, 30000);\n" : "process.stderr.write('diagnostic 日本語\\n'); process.exit(7);\n");
    if (mode == "invalid utf8") source = language == "python" ? "import sys\nsys.stdout.buffer.write(bytes([0xe3,0x81]))\n" : "process.stdout.write(Buffer.from([0xe3,0x81]));\n";
    const auto file = scriptFile(project.path(), "failure" + extension(language), source); QVERIFY(!file.isEmpty());
    auto flow = scriptFlow(language, mode == "missing file" ? "absent.py" : file);
    if (mode == "missing directory") flow.nodes[3].scriptDirectory = "absent directory";
    Runner runner; QSignalSpy done(&runner, &Runner::finished); AgentConfig config;
    QVERIFY(runner.start(flow, project.path(), config));
    if (mode == "no deadline") { QTest::qWait(1200); QVERIFY(done.isEmpty()); QVERIFY(runner.running()); QVERIFY(runner.findChildren<QTimer *>().isEmpty()); }
    if (sleep) { QTRY_VERIFY(runner.results().at(3).state == State::Running); runner.cancel(); }
    QTRY_COMPARE(done.count(), 1); QVERIFY(!done[0][0].toBool()); QVERIFY(!runner.running());
    QVERIFY(runner.results().at(2).state != State::Succeeded);
    QVERIFY(runner.results().at(3).state == (sleep ? State::Cancelled : State::Failed));
    if (mode == "exit") { QVERIFY(runner.results().at(3).log.contains("diagnostic 日本語")); QVERIFY(runner.results().at(3).log.contains("exit 7")); }
    if (mode == "invalid utf8") QVERIFY(runner.results().at(3).log.contains("UTF-8"));
}
void ScriptTest::loop_data() {
    QTest::addColumn<QString>("language"); QTest::newRow("Python") << QString("python"); QTest::newRow("JavaScript") << QString("javascript");
}
void ScriptTest::loop() {
    QFETCH(QString, language); if (!available(language)) QSKIP("System runtime not installed"); QTemporaryDir project;
    const QString source = language == "python" ? "import json,sys\nv=json.load(sys.stdin)\nprint(json.dumps({'count':v.get('count',0)+1}))\n" :
        "const v=JSON.parse(require('node:fs').readFileSync(0,'utf8')); console.log(JSON.stringify({count:(v.count||0)+1}));\n";
    const auto file = scriptFile(project.path(), "counter" + extension(language), source); QVERIFY(!file.isEmpty());
    auto flow = scriptFlow(language, file); Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"count", "3", false}}; flow.nodes[4] = branch;
    flow.edges = {{1, 3}, {3, 4}, {4, 3, 0, 0}, {4, 2, 1, 0}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
    connect(&runner, &Runner::executionChanged, this, [&](const NodeExecution &entry) { if (entry.node == 3 && entry.state == State::Succeeded) history.push_back(entry); });
    QVERIFY2(runner.start(flow, project.path(), {}), qPrintable(flow.validate().join('\n'))); QTRY_COMPARE(done.count(), 1); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QCOMPARE(runner.results().at(3).executions, quint64(3)); QVERIFY(runner.results().at(2).output == Json({{"count", 3}}));
    QCOMPARE(history.size(), size_t(3)); for (size_t i = 0; i < history.size(); ++i) { QCOMPARE(history[i].executionNumber, quint64(i + 1)); QVERIFY(history[i].output == Json({{"count", i + 1}})); }
}
void ScriptTest::parallelJoin() {
    if (!available("python") || !available("javascript")) QSKIP("System runtimes not installed"); QTemporaryDir project;
    const auto python = scriptFile(project.path(), "one.py", outputSource("python", "{\"a\":1}"));
    const auto javascript = scriptFile(project.path(), "two.js", outputSource("javascript", "{\"b\":2}")); QVERIFY(!python.isEmpty() && !javascript.isEmpty());
    auto flow = scriptFlow("python", python); Node second = flow.nodes[3]; second.id = 4; second.scriptLanguage = "javascript"; second.scriptPath = javascript; flow.nodes[4] = second;
    Node join; join.id = 5; join.kind = Kind::And; flow.nodes[5] = join; flow.edges = {{1, 3}, {1, 4}, {3, 5, 0, 0}, {4, 5, 0, 1}, {5, 2}};
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY(runner.start(flow, project.path(), {})); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QVERIFY(runner.results().at(2).output == Json::array({{{"a", 1}}, {{"b", 2}}}));
}
void ScriptTest::outputLimit() {
    if (!available("python")) QSKIP("System Python not installed"); QTemporaryDir project;
    const auto file = scriptFile(project.path(), "large.py", "import sys\nsys.stdout.write('x' * (33 * 1024 * 1024))\n"); QVERIFY(!file.isEmpty());
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY(runner.start(scriptFlow("python", file), project.path(), {})); QTRY_COMPARE(done.count(), 1);
    QVERIFY(!done[0][0].toBool()); QVERIFY(runner.results().at(3).log.contains("32MiB")); QVERIFY(runner.results().at(3).log.size() < 1000);
}
void ScriptTest::example() {
    if (!available("python") || !available("javascript")) QSKIP("System runtimes not installed");
    const QString root = AGENTNODIFY_SOURCE_DIR; const auto flow = Flow::load(root + "/examples/script.flow.json");
    Runner runner; QSignalSpy done(&runner, &Runner::finished); QVERIFY(runner.start(flow, root, {})); QTRY_COMPARE(done.count(), 1);
    QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QVERIFY(runner.results().at(2).output == Json({{"python", "Pythonからの出力"}, {"javascript", "Node.jsからの出力"}}));
}
}
QTEST_GUILESS_MAIN(an::ScriptTest)
