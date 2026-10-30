#include "core/runner.h"
#include "core/history.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <set>
using namespace an;
namespace {
QString countSchema() { return R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"],"additionalProperties":false})"; }
AgentConfig config() {
    auto path = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    path += ".exe";
#endif
    return {path, path, path};
}
Node agent(Id id, QString name, bool retain = true, bool structured = true) {
    Node node; node.id = id; node.kind = Kind::Agent; node.agent = name; node.model = "fixture";
    node.prompt = "CONTEXT_FIXTURE"; node.effort = "high"; node.retainContext = retain; if (structured) node.schema = countSchema(); return node;
}
Flow loop(QString name, bool retain = true, bool structured = true, QString extra = {}) {
    auto flow = Flow::initial(); flow.nodes[3] = agent(3, name, retain, structured); flow.nodes[3].prompt += extra;
    auto counter = agent(4, "Codex", false); counter.prompt = "COUNT_TO_3"; flow.nodes[4] = counter;
    Node branch; branch.id = 5; branch.kind = Kind::Branch; branch.rules = {{"count", "3", false}}; flow.nodes[5] = branch;
    flow.edges = {{1,3},{3,4},{4,5},{5,3,0,0},{5,2,1,0}}; return flow;
}
std::vector<Json> trace(const QString &directory) {
    QFile file(QDir(directory).filePath("context-trace.jsonl")); std::vector<Json> entries;
    if (file.open(QIODevice::ReadOnly)) for (const auto &line : file.readAll().split('\n')) if (!line.isEmpty()) entries.push_back(Json::parse(line.toStdString()));
    return entries;
}
Json value(const NodeExecution &entry) { return entry.output.is_string() ? Json::parse(entry.output.get<std::string>()) : entry.output; }
}
class ContextTest : public QObject {
    Q_OBJECT
private slots:
    void legacyHistoryMetadata() {
        QTemporaryDir directory; NodeExecution entry; entry.id="12345678-1234-1234-1234-123456789abc"; entry.startedAt=QDateTime::currentDateTimeUtc(); entry.state=State::Succeeded;
        saveNodeExecution(directory.path(),entry); QFile file(directory.filePath(entry.id+".json")); QVERIFY(file.open(QIODevice::ReadOnly)); auto json=Json::parse(file.readAll().toStdString()); file.close();
        json.erase("model"); json.erase("effort"); json.erase("retainContext"); json.erase("contextResumed");
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate)); file.write(QByteArray::fromStdString(json.dump())); file.close();
        QStringList errors; auto saved=loadNodeExecutions(directory.path(),&errors); QVERIFY(errors.isEmpty()); QCOMPARE(saved.size(),size_t(1)); QVERIFY(saved[0].model.isEmpty()); QVERIFY(saved[0].effort.isEmpty()); QVERIFY(!saved[0].retainContext); QVERIFY(!saved[0].contextResumed);
    }
    void settingsRoundtrip() {
        auto flow = loop("Codex"); const auto json = flow.toJson(); QVERIFY(Flow::fromJson(json).nodes.at(3).retainContext);
        auto legacy = json; for (auto &node : legacy["nodes"]) node.erase("retainContext");
        QVERIFY(!Flow::fromJson(legacy).nodes.at(3).retainContext);
        flow.nodes[3].retainContext = false; QVERIFY(!Flow::fromJson(flow.toJson()).nodes.at(3).retainContext);
    }
    void loopContexts_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<bool>("retain"); QTest::addColumn<bool>("structured");
        for (QString name : {"Codex", "Claude Code", "OpenCode"}) for (bool retain : {false,true}) for (bool structured : {false,true})
            QTest::newRow(qPrintable(name + (retain ? " retained" : " independent") + (structured ? " JSON" : " text"))) << name << retain << structured;
    }
    void loopContexts() {
        QFETCH(QString, name); QFETCH(bool, retain); QFETCH(bool, structured); QTemporaryDir directory; Runner runner;
        const auto flow = loop(name, retain, structured); QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
        connect(&runner, &Runner::executionChanged, &runner, [&](const NodeExecution &entry) { if (entry.node == 3 && entry.state == State::Succeeded) history.push_back(entry); });
        std::set<QString> ids;
        for (int run = 0; run < 2; ++run) {
            if (run) QVERIFY(QFile::remove(directory.filePath("counter.txt")));
            done.clear(); QVERIFY(runner.start(flow, directory.path(), config())); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
            QCOMPARE(runner.results().at(3).executions, quint64(3));
            for (int i = 0; i < 3; ++i) {
                const auto &entry = history.at(size_t(run * 3 + i)); QVERIFY(ids.insert(entry.id).second); QCOMPARE(entry.executionNumber, quint64(i + 1)); QVERIFY(value(entry).at("count") == (retain ? i + 1 : 1));
                QCOMPARE(entry.retainContext,retain); QCOMPARE(entry.contextResumed,retain && i>0);
                QCOMPARE(entry.model,QString("fixture")); QCOMPARE(entry.effort,QString("high"));
                const auto header=Json::parse(entry.log.section('\n',0,0).toStdString()); QVERIFY(header.at("type")=="agentnodify.prompt");
                QString expected=flow.nodes.at(3).prompt;
                if(name=="OpenCode" && structured) expected+="\nReturn only JSON (no Markdown) matching this JSON Schema:\n"+countSchema();
                QCOMPARE(QString::fromStdString(header.at("prompt").get<std::string>()),expected);
                saveNodeExecution(directory.filePath("history"),entry);
            }
        }
        const auto entries = trace(directory.path()); QCOMPARE(entries.size(), size_t(6));
        QStringList errors; const auto saved=loadNodeExecutions(directory.filePath("history"),&errors); QVERIFY(errors.isEmpty()); QCOMPARE(saved.size(),history.size());
        for(const auto &entry:saved) { QCOMPARE(entry.retainContext,retain); QCOMPARE(entry.contextResumed,retain && entry.executionNumber>1); QVERIFY(entry.log.startsWith("{\"prompt\":")); QCOMPARE(entry.model,QString("fixture")); QCOMPARE(entry.effort,QString("high")); }
        for (size_t i = 0; i < entries.size(); ++i) {
            const bool resumed = retain && i % 3 != 0; QVERIFY(entries[i].at("resumed") == resumed);
            if (resumed) QVERIFY(entries[i].at("session") == entries[i-1].at("session"));
            else if (i) QVERIFY(entries[i].at("session") != entries[i-1].at("session"));
            const auto args = entries[i].at("args");
            QVERIFY(std::find(args.begin(), args.end(), "--last") == args.end()); QVERIFY(std::find(args.begin(), args.end(), "--continue") == args.end());
            if (name == "Codex") {
                auto resume = std::find(args.begin(), args.end(), "resume"); QVERIFY((resume != args.end()) == resumed);
                const auto sandbox = std::find(args.begin(), args.end(), "--sandbox"); QVERIFY(sandbox != args.end()); QVERIFY(*(sandbox+1) == "workspace-write");
                if (resumed) { QVERIFY(sandbox < resume); QVERIFY(*(resume+1) == entries[i].at("session")); QVERIFY(args.back() == "-"); }
            } else {
                const auto flag = name == "OpenCode" ? "--session" : "--resume"; const auto pos = std::find(args.begin(), args.end(), flag);
                QVERIFY((pos != args.end()) == resumed); if (resumed) QVERIFY(*(pos+1) == entries[i].at("session"));
            }
        }
    }
    void disabledNeedsNoSession_data() { isolation_data(); }
    void disabledNeedsNoSession() {
        QFETCH(QString,name); QTemporaryDir directory; Runner runner; QSignalSpy done(&runner,&Runner::finished);
        QVERIFY(runner.start(loop(name,false,true," OMIT_SESSION"),directory.path(),config())); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool());
        QCOMPARE(runner.results().at(3).executions,quint64(3)); QVERIFY(runner.results().at(3).output.at("count") == 1);
    }
    void isolation_data() {
        QTest::addColumn<QString>("name"); for (QString name : {"Codex","Claude Code","OpenCode"}) QTest::newRow(qPrintable(name)) << name;
    }
    void isolation() {
        QFETCH(QString, name); QTemporaryDir directory; auto flow = Flow::initial();
        flow.nodes[3] = agent(3,name); flow.nodes[4] = agent(4,name);
        Node join; join.id = 5; join.kind = Kind::All; flow.nodes[5] = join;
        Node pass; pass.id = 6; pass.kind = Kind::Branch; flow.nodes[6] = pass;
        flow.edges = {{1,3},{1,4},{1,6},{6,3},{6,4},{3,5,0,0},{4,5,0,1},{5,2}};
        Runner first, second; QSignalSpy a(&first,&Runner::finished), b(&second,&Runner::finished);
        QVERIFY(first.start(flow,directory.path(),config())); QVERIFY(second.start(flow,directory.path(),config()));
        QTRY_COMPARE(a.count(),1); QTRY_COMPARE(b.count(),1); QVERIFY(a[0][0].toBool()); QVERIFY(b[0][0].toBool());
        const auto entries = trace(directory.path()); QCOMPARE(entries.size(),size_t(8)); std::map<std::string,int> sessions;
        for (const auto &entry : entries) ++sessions[entry.at("session").get<std::string>()];
        QCOMPARE(sessions.size(),size_t(4)); for (const auto &[id,count] : sessions) QCOMPARE(count,2);
        for (Runner *runner : {&first,&second}) { QCOMPARE(runner->results().at(3).executions,quint64(2)); QVERIFY(runner->results().at(3).output.at("count") == 2); QVERIFY(runner->results().at(4).output.at("count") == 2); }
    }
    void errors_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("mode");
        for (QString name : {"Codex","Claude Code","OpenCode"}) for (QString mode : {"OMIT_SESSION","CHANGE_SESSION","RESUME_ERROR"}) QTest::newRow(qPrintable(name + " " + mode)) << name << mode;
    }
    void errors() {
        QFETCH(QString,name); QFETCH(QString,mode); QTemporaryDir directory; Runner runner; QSignalSpy done(&runner,&Runner::finished);
        QVERIFY(runner.start(loop(name,true,true," " + mode),directory.path(),config())); QTRY_COMPARE(done.count(),1); QVERIFY(!done[0][0].toBool());
        QVERIFY(runner.results().at(3).state == State::Failed); QCOMPARE(runner.results().at(3).executions, mode == "OMIT_SESSION" ? quint64(1) : quint64(2));
        if (mode != "RESUME_ERROR") QVERIFY(done[0][1].toString().contains("セッションID"));
    }
    void cancellationClearsContext_data() { isolation_data(); }
    void cancellationClearsContext() {
        QFETCH(QString,name); QTemporaryDir directory; Runner runner; QSignalSpy done(&runner,&Runner::finished);
        connect(&runner,&Runner::executionChanged,&runner,[&](const NodeExecution &entry) { if (entry.node == 3 && entry.executionNumber == 2 && entry.state == State::Running) QTimer::singleShot(30,&runner,&Runner::cancel); });
        QVERIFY(runner.start(loop(name,true,true," CANCEL_DELAY"),directory.path(),config())); QTRY_COMPARE(done.count(),1); QVERIFY(!done[0][0].toBool());
        QVERIFY(QFile::remove(directory.filePath("counter.txt"))); auto fresh = Flow::initial(); fresh.nodes[3] = agent(3,name); fresh.edges = {{1,3},{3,2}};
        done.clear(); QVERIFY(runner.start(fresh,directory.path(),config())); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(runner.results().at(3).output.at("count") == 1);
    }
    void sessionMetadata() {
        const QString id = "12345678-1234-1234-1234-123456789abc";
        QCOMPARE(decodeAgentSessionId("Codex", QByteArray("{\"type\":\"thread.started\",\"thread_id\":\"") + id.toUtf8() + "\"}\n"), id);
        QVERIFY(decodeAgentSessionId("Codex", R"({"type":"item.completed","thread_id":"not-metadata"})").isEmpty());
        QVERIFY_EXCEPTION_THROWN(decodeAgentSessionId("Codex", R"({"type":"thread.started","thread_id":"--last"})"), std::exception);
        QVERIFY_EXCEPTION_THROWN(decodeAgentSessionId("OpenCode", R"({"type":"text","sessionID":"ses_one"}
{"type":"step_finish","sessionID":"ses_two"})"), std::exception);
    }
};
QTEST_GUILESS_MAIN(ContextTest)
#include "context_test.moc"
