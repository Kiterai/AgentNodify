#include "coordinator.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
using namespace an;
namespace {
Node operation(Kind kind, QString flow = "child", QString path = "target") {
    Node node; node.kind = kind; node.targetFlow = flow; node.projectPath = path; return node;
}
Flow chain(std::initializer_list<Node> nodes) {
    auto flow = Flow::initial(); Id previous = 1, id = 3;
    for (auto node : nodes) { node.id = id; flow.nodes[id] = node; flow.edges.push_back({previous, id}); previous = id++; }
    flow.edges.push_back({previous, 2}); return flow;
}
Flow child(QString prompt = "SHORT_DELAY") {
    Node node; node.kind = Kind::Agent; node.model = "fixture"; node.prompt = prompt;
    node.schema = R"({"type":"object","properties":{"result":{"type":"string"}},"required":["result"]})";
    return chain({node});
}
void configure(Coordinator &host, const QString &directory, Flow parent, Flow target = child()) {
    auto program = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    program += ".exe";
#endif
    host.config = {program, program, program};
    host.flows = {{"parent", parent}, {"child", target}};
    host.projects = {{"root", "parent", directory}};
}
}
class CoordinatorTest : public QObject {
    Q_OBJECT
private slots:
    void serializationAndValidation() {
        auto flow = chain({operation(Kind::ProjectCreate), operation(Kind::FlowRun, "child", "${path}"), operation(Kind::FlowWait)});
        QVERIFY(flow.validate().isEmpty());
        const auto restored = Flow::fromJson(flow.toJson()); QVERIFY(restored.toJson() == flow.toJson());
        flow.nodes[3].projectPath = "${missing}"; QVERIFY(!flow.validate().isEmpty());
        flow.nodes[3].projectPath = "target"; flow.nodes[3].targetFlow.clear(); QVERIFY(!flow.validate().isEmpty());
        // Wait output is dynamic, not inferred from its input.
        Node branch; branch.kind = Kind::Branch; branch.rules = {{"arbitrary.nested[0]", "x", true}};
        flow = chain({operation(Kind::FlowWait), branch}); flow.edges.push_back({4, 2, 1, 0}); QVERIFY(flow.validate().isEmpty());
    }
    void create_data() {
        QTest::addColumn<QString>("mode");
        for (QString mode : {"success", "duplicate", "missing flow", "file", "bad ancestor", "invalid path"}) QTest::newRow(qPrintable(mode)) << mode;
    }
    void create() {
        QFETCH(QString, mode); QTemporaryDir directory; Coordinator host; Runner runner;
        auto flow = chain({operation(Kind::ProjectCreate)}); configure(host, directory.path(), flow);
        if (mode == "duplicate") host.projects.push_back({"existing", "child", directory.filePath("./target/../target")});
        if (mode == "missing flow") host.flows.erase("child");
        if (mode == "invalid path") host.flows["parent"].nodes[3].projectPath = QString(QChar(0));
        if (mode == "file" || mode == "bad ancestor") {
            QFile file(directory.filePath("target")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
            if (mode == "bad ancestor") host.flows["parent"].nodes[3].projectPath = "target/sub";
        }
        QSignalSpy done(&runner, &Runner::finished), created(&host, &Coordinator::projectCreated);
        QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
        const auto output = runner.results().at(2).output;
        if (mode == "success") {
            QCOMPARE(output.at("result"), Json("success")); QCOMPARE(output.size(), size_t(2));
            QCOMPARE(output.at("path"), Json(Coordinator::normalizedPath(directory.filePath("target")).toStdString()));
            QVERIFY(QFileInfo(directory.filePath("target")).isDir()); QCOMPARE(created.count(), 1);
        } else {
            QCOMPARE(output.at("result"), Json("failed")); QCOMPARE(created.count(), 0);
            const QString expected = mode == "duplicate" ? "project_already_exists" : mode == "missing flow" ? "flow_not_found" : mode == "file" ? "path_not_directory" : mode == "invalid path" ? "invalid_path" : "directory_create_failed";
            QCOMPARE(output.at("reason"), Json(expected.toStdString()));
        }
    }
    void startAlreadyWaitAndCached() {
        QTemporaryDir directory; Coordinator host; Runner runner;
        configure(host, directory.path(), chain({operation(Kind::ProjectCreate), operation(Kind::FlowRun, "child", "${path}"), operation(Kind::FlowRun), operation(Kind::FlowWait), operation(Kind::FlowWait)}));
        QSignalSpy done(&runner, &Runner::finished); std::vector<std::pair<QString, NodeExecution>> history;
        connect(&host, &Coordinator::executionChanged, &host, [&](QString id, NodeExecution entry) { history.push_back({id, entry}); });
        QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000); QVERIFY(done[0][0].toBool());
        QCOMPARE(runner.results().at(4).output, Json({{"result", "started"}}));
        QCOMPARE(runner.results().at(5).output, Json({{"result", "already"}}));
        QCOMPARE(runner.results().at(6).output, Json({{"result", "ok"}})); QCOMPARE(runner.results().at(7).output, runner.results().at(6).output);
        QVERIFY(!host.running()); QVERIFY(history.size() >= 16);
        std::set<QString> childIds;
        for (const auto &[id, entry] : history) if (id != "root") { childIds.insert(id); QVERIFY(entry.directory != directory.path()); }
        QCOMPARE(childIds.size(), size_t(1));
        // Restart uses a fresh invocation and clears the cached output.
        host.flows["child"] = child("RESPONSE_TWO"); host.flows["child"].nodes[3].schema.clear();
        host.flows["parent"] = chain({operation(Kind::FlowRun), operation(Kind::FlowWait)});
        done.clear(); QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool()); QCOMPARE(runner.results().at(2).output, Json("two"));
    }
    void waitErrors_data() {
        QTest::addColumn<QString>("mode");
        for (QString mode : {"missing project", "not started", "failure", "cancel", "self", "cycle"}) QTest::newRow(qPrintable(mode)) << mode;
    }
    void waitErrors() {
        QFETCH(QString, mode); QTemporaryDir directory; Coordinator host; Runner runner;
        auto flow = chain({operation(Kind::FlowWait)}); configure(host, directory.path(), flow);
        QVERIFY(QDir().mkpath(directory.filePath("target")));
        if (mode != "missing project") host.projects.push_back({"target", "child", directory.filePath("target")});
        QString expected = mode == "missing project" ? "project_not_found" : "execution_not_started";
        if (mode == "self") { host.flows["parent"] = chain({operation(Kind::FlowWait, "parent", ".")}); expected = "wait_cycle"; }
        if (mode == "failure" || mode == "cancel" || mode == "cycle") {
            host.flows["parent"] = chain({operation(Kind::FlowRun), operation(Kind::FlowWait)});
            host.flows["child"] = mode == "cycle" ? chain({operation(Kind::FlowWait, "parent", "..")}) : child(mode == "failure" ? "FAIL" : "SLEEP");
            expected = mode == "cycle" ? "wait_cycle" : mode == "failure" ? "target_failed" : "target_cancelled";
        }
        QSignalSpy done(&runner, &Runner::finished);
        if (mode == "cancel") connect(&host, &Coordinator::executionChanged, &host, [&](QString id, NodeExecution entry) {
            if (id != "root" && entry.node == 3 && entry.state == State::Running) QTimer::singleShot(20, &host, [&] {
                for (auto *childRunner : host.findChildren<Runner *>()) if (childRunner->running()) childRunner->cancel();
            });
        });
        QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000);
        QVERIFY(!done[0][0].toBool()); QVERIFY2(done[0][1].toString().contains(expected), qPrintable(done[0][1].toString())); QVERIFY(!host.running());
    }
    void startFailure_data() {
        QTest::addColumn<QString>("mode");
        for (QString mode : {"missing project", "missing flow", "invalid target", "missing directory", "missing model"}) QTest::newRow(qPrintable(mode)) << mode;
    }
    void startFailure() {
        QFETCH(QString, mode); QTemporaryDir directory; Coordinator host; Runner runner;
        configure(host, directory.path(), chain({operation(Kind::FlowRun)}));
        if (mode != "missing project") host.projects.push_back({"target", "child", directory.filePath("target")});
        if (mode != "missing directory") QVERIFY(QDir().mkpath(directory.filePath("target")));
        if (mode == "missing flow") host.flows.erase("child");
        if (mode == "invalid target") host.flows["child"].edges.clear();
        if (mode == "missing model") host.flows["child"].nodes[3].model.clear();
        QSignalSpy done(&runner, &Runner::finished); QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1);
        QVERIFY(done[0][0].toBool()); QCOMPARE(runner.results().at(2).output, Json({{"result", "failed"}})); QVERIFY(!host.running());
    }
    void multipleWaiters() {
        QTemporaryDir directory; Coordinator host; Runner first, second;
        configure(host, directory.path(), chain({operation(Kind::FlowWait, "child", ".")}));
        host.flows["other"] = host.flows["parent"];
        host.projects.push_back({"other", "other", directory.path()}); host.projects.push_back({"target", "child", directory.path()});
        QVERIFY(host.start(host.projects.back()));
        QSignalSpy a(&first, &Runner::finished), b(&second, &Runner::finished);
        QVERIFY(host.start(host.projects[0], &first)); QVERIFY(host.start(host.projects[1], &second));
        QTRY_COMPARE(a.count(), 1); QTRY_COMPARE(b.count(), 1); QVERIFY(a[0][0].toBool()); QVERIFY(b[0][0].toBool());
        QCOMPARE(first.results().at(2).output, Json({{"result", "ok"}})); QCOMPARE(first.results().at(2).output, second.results().at(2).output);
    }
    void parallelAndJoin() {
        QTemporaryDir directory; Coordinator host; Runner runner;
        auto flow = Flow::initial();
        for (Id id : {3u, 4u}) { auto node = operation(Kind::FlowRun, "child", id == 3 ? "a" : "b"); node.id = id; flow.nodes[id] = node; }
        for (Id id : {5u, 6u}) { auto node = operation(Kind::FlowWait, "child", id == 5 ? "a" : "b"); node.id = id; flow.nodes[id] = node; }
        Node join; join.id = 7; join.kind = Kind::All; flow.nodes[7] = join;
        flow.edges = {{1,3},{1,4},{3,5},{4,6},{5,7,0,0},{6,7,0,1},{7,2}};
        configure(host, directory.path(), flow);
        for (QString name : {"a", "b"}) { QVERIFY(QDir().mkpath(directory.filePath(name))); host.projects.push_back({name, "child", directory.filePath(name)}); }
        QSignalSpy done(&runner, &Runner::finished); QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
        QCOMPARE(runner.results().at(2).output, Json::array({Json{{"result","ok"}}, Json{{"result","ok"}}}));
    }
    void loopAndSeparateHistory() {
        QTemporaryDir directory; Coordinator host; Runner runner;
        Node branch; branch.kind = Kind::Branch; branch.rules = {{"result", "success", true}};
        auto flow = chain({operation(Kind::ProjectCreate), branch}); flow.edges.push_back({4,3,0,0});
        flow.edges[2].output = 1;
        configure(host, directory.path(), flow); QSignalSpy done(&runner, &Runner::finished); std::vector<NodeExecution> history;
        connect(&host, &Coordinator::executionChanged, &host, [&](QString, NodeExecution entry) { if (entry.node == 3 && entry.state == State::Succeeded) history.push_back(entry); });
        QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
        QCOMPARE(history.size(), size_t(2)); QVERIFY(history[0].id != history[1].id); QCOMPARE(history[0].executionNumber, quint64(1)); QCOMPARE(history[1].executionNumber, quint64(2));
        QCOMPARE(history[0].output.at("result"), Json("success")); QCOMPARE(history[1].output.at("reason"), Json("project_already_exists"));
    }
    void detachedRunKeepsActive() {
        QTemporaryDir directory; Coordinator host; Runner runner;
        configure(host, directory.path(), chain({operation(Kind::FlowRun, "child", ".")}));
        host.flows["child"] = child("SLEEP"); host.projects.push_back({"target", "child", directory.path()});
        QSignalSpy done(&runner, &Runner::finished); QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1);
        QVERIFY(done[0][0].toBool()); QVERIFY(host.running()); host.cancelAll(); QVERIFY(!host.running());
    }
    void destroyedTargetUnblocksWaiters() {
        QTemporaryDir directory; Coordinator host; Runner waiter;
        configure(host, directory.path(), chain({operation(Kind::FlowWait, "child", ".")}));
        host.flows["child"] = child("SLEEP"); host.projects.push_back({"target", "child", directory.path()});
        auto *target = new Runner; QVERIFY(host.start(host.projects.back(), target));
        QSignalSpy done(&waiter, &Runner::finished); QVERIFY(host.start(host.projects.front(), &waiter));
        QTRY_VERIFY(waiter.results().at(3).state == State::Running); delete target;
        QTRY_COMPARE(done.count(), 1); QVERIFY(!done[0][0].toBool()); QVERIFY(done[0][1].toString().contains("target_cancelled")); QVERIFY(!host.running());
    }
    void cancelWaiterAndRestart() {
        QTemporaryDir directory; Coordinator host; Runner runner;
        configure(host, directory.path(), chain({operation(Kind::FlowWait)}));
        host.projects.push_back({"target", "child", directory.path()}); host.flows["parent"].nodes[3].projectPath = ".";
        QVERIFY(host.start(host.projects.back())); QVERIFY(host.start(host.projects.front(), &runner));
        QTRY_VERIFY(runner.results().at(3).state == State::Running); runner.cancel();
        host.flows["parent"] = chain({}); QSignalSpy done(&runner, &Runner::finished);
        QVERIFY(host.start(host.projects.front(), &runner)); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
        QTRY_VERIFY(!host.running()); QCOMPARE(done.count(), 1); QCOMPARE(runner.results().at(2).output, Json::object());
    }
};
QTEST_GUILESS_MAIN(CoordinatorTest)
#include "coordinator_test.moc"
