// Opt-in integration runner: NOT registered with CTest; invokes paid services.
#include "runner.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace an;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOptions({
        {{"a", "agent"}, "Codex / Claude Code / OpenCode", "name"},
        {{"p", "program"}, "Native CLI executable", "path"},
        {{"m", "model"}, "Explicit model ID (no default/fallback)", "model"},
        {{"c", "case"}, "text / json / loop / and", "case", "text"},
        {{"r", "report"}, "Write JSON test report", "path"},
        {{"t", "timeout"}, "Per-node timeout in seconds", "seconds", "90"}
    }); parser.process(app);
    const auto agent = parser.value("agent"), model = parser.value("model"), test = parser.value("case");
    if ((agent != "Codex" && agent != "Claude Code" && agent != "OpenCode") ||
        model.isEmpty() || parser.value("program").isEmpty() ||
        (test != "text" && test != "json" && test != "loop" && test != "and")) {
        QTextStream(stderr) << "Specify --agent, --program, --model and --case text/json/loop/and\n"; return 2;
    }
    const auto lower = model.toLower();
    const auto modelId = lower.section('/', -1);
    if (modelId != "gpt-6.0-luna" && modelId != "gpt-6-luna" && modelId != "claude-haiku-5-5") {
        QTextStream(stderr) << "Only the models authorized in spec/spec.md are permitted\n"; return 2;
    }
    Flow flow = Flow::initial(); flow.name = "Live " + agent + " " + test;
    const QString noTools = "Do not use tools, read files, or modify files. ";
    const QString countSchema = R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"],"additionalProperties":false})";
    auto agentNode = [&](Id id, const QString &prompt, const QString &schema) {
        Node n; n.id = id; n.kind = Kind::Agent; n.title = "Live test"; n.agent = agent;
        n.model = model; n.prompt = noTools + prompt; n.schema = schema; flow.nodes[id] = n;
    };
    Json expected;
    if (test == "text") {
        agentNode(3, "Reply with exactly AGENTNODIFY_OK, no punctuation or explanation.", "");
        flow.edges = {{1, 3}, {3, 2}}; expected = "AGENTNODIFY_OK";
    } else if (test == "json") {
        agentNode(3, "Return JSON with count equal to 1.", countSchema);
        flow.edges = {{1, 3}, {3, 2}}; expected = {{"count", 1}};
    } else if (test == "loop") {
        agentNode(3, "Return JSON with count equal to 0.", countSchema);
        agentNode(4, "The input count is ${count}. Return JSON with count equal to that integer plus 1.", countSchema);
        Node branch; branch.id = 5; branch.kind = Kind::Branch; branch.rules = {{"count", "2", false}}; flow.nodes[5] = branch;
        flow.edges = {{1, 3}, {3, 4}, {4, 5}, {5, 4, 0, 0}, {5, 2, 1, 0}}; expected = {{"count", 2}};
    } else {
        agentNode(3, "Return JSON with count equal to 0.", countSchema);
        Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"count", "2", false}}; flow.nodes[4] = branch;
        agentNode(5, "The input count is ${count}. Return JSON with count equal to that integer plus 1.", countSchema);
        agentNode(6, "The input count is ${count}. Return JSON with count equal to that integer plus 1.", countSchema);
        Node join; join.id = 7; join.kind = Kind::And; flow.nodes[7] = join;
        agentNode(8, "Return JSON with count equal to ${[0].count}. The other input is ${[1].count}; do not add them.", countSchema);
        flow.edges = {{1, 3}, {3, 4}, {4, 5, 0, 0}, {4, 6, 0, 0}, {5, 7, 0, 0}, {6, 7, 0, 1}, {7, 8}, {8, 4}, {4, 2, 1, 0}};
        expected = {{"count", 2}};
    }
    QTemporaryDir directory; Runner runner;
    AgentConfig config; config.codex = config.claude = config.opencode = parser.value("program");
    config.timeoutSeconds = qBound(1, parser.value("timeout").toInt(), 600);
    Json events = Json::array();
    QObject::connect(&runner, &Runner::fired, &app, [&](Id id, quint64 execution, const QString &json) {
        events.push_back({{"node", id}, {"execution", execution}, {"output", parseJson(json)}});
        // Bound mistakes by the model as well as unbounded graphs in live tests.
        if (events.size() > 50) runner.cancel();
    });
    QObject::connect(&runner, &Runner::finished, &app, [&](bool ok, const QString &message) {
        Json actual; if (runner.results().contains(2)) actual = runner.results().at(2).output;
        if (test == "text" && actual.is_string()) actual = actual.get<std::string>();
        bool passed = ok && (test == "text" ? display(actual).trimmed() == display(expected) : actual == expected);
        if (passed && test == "loop") passed = runner.results().at(4).executions == 2;
        if (passed && test == "and") passed = runner.results().at(7).executions == 2;
        Json results = Json::array();
        for (auto &[id, result] : runner.results()) results.push_back({{"id", id}, {"state", stateName(result.state).toStdString()},
            {"executions", result.executions}, {"output", result.output}, {"log", result.log.toStdString()}});
        Json report = {{"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()},
            {"agent", agent.toStdString()}, {"model", model.toStdString()}, {"case", test.toStdString()},
            {"passed", passed}, {"message", message.toStdString()}, {"expected", expected}, {"actual", actual},
            {"events", events}, {"nodes", results}};
        if (!parser.value("report").isEmpty()) {
            QSaveFile file(parser.value("report")); auto bytes = QByteArray::fromStdString(report.dump(2));
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) passed = false;
        }
        QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << agent << " " << test << ": " << message << '\n';
        app.exit(passed ? 0 : 1);
    });
    // start may emit finished synchronously, so invoke after entering the loop.
    QTimer::singleShot(0, &app, [&] { runner.start(flow, directory.path(), config); });
    QTimer::singleShot(480000, &runner, [&] { if (runner.running()) runner.cancel(); });
    return app.exec();
}
