#include "core/runner.h"
#include "core/coordinator.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
using namespace an;
namespace {
Node inputNode(Id id = 3) { Node node; node.id = id; node.kind = Kind::UserInput; node.userChoices = {"Continue", "Finish"}; return node; }
Flow branching() {
    auto flow = Flow::initial(); flow.nodes[3] = inputNode();
    for (Id id : {4u,5u}) { Node pass; pass.id = id; pass.kind = Kind::Branch; flow.nodes[id] = pass; }
    flow.edges = {{1,3},{3,4,0,0},{3,5,1,0},{4,2},{5,2}}; return flow;
}
}
class UserInputTest : public QObject {
    Q_OBJECT
private slots:
    void serializationAndValidation() {
        QCOMPARE(Node().userChoices, QStringList{"OK"});
        auto flow = branching(); QVERIFY(flow.validate().isEmpty());
        flow.nodes[3].userChoices = {"はい 日本語", "いいえ"}; QVERIFY(Flow::fromJson(flow.toJson()).toJson() == flow.toJson());
        auto legacy = flow.toJson(); legacy["nodes"][2].erase("userChoices"); QCOMPARE(Flow::fromJson(legacy).nodes.at(3).userChoices, QStringList{"OK"});
        flow.nodes[3].userChoices.clear(); QVERIFY(!flow.validate().isEmpty()); flow.nodes[3].userChoices = {" ", "No"}; QVERIFY(!flow.validate().isEmpty());
        flow = branching(); flow.edges.erase(flow.edges.begin() + 2); QStringList warnings; QVERIFY(flow.validate(&warnings).isEmpty()); QVERIFY(!warnings.isEmpty());
        flow = branching(); Node agent; agent.id = 6; agent.kind = Kind::Agent; agent.prompt = "${original}"; flow.nodes[6] = agent;
        flow.edges = {{1,3},{3,6,0,0},{3,6,1,0},{6,2}}; flow.nodes.erase(4); flow.nodes.erase(5); QVERIFY(!flow.validate().isEmpty());
    }
    void waitAndBranch_data() {
        QTest::addColumn<unsigned>("choice"); QTest::addColumn<QString>("text");
        QTest::newRow("first") << 0u << QString("日本語\nmultiple lines"); QTest::newRow("second JSON-looking string") << 1u << QString("{\"answer\":42}"); QTest::newRow("empty") << 1u << QString();
    }
    void waitAndBranch() {
        QFETCH(unsigned,choice); QFETCH(QString,text); QTemporaryDir directory; Runner runner; auto flow = branching();
        Node source; source.id = 6; source.kind = Kind::FlowWait; source.targetFlow = "fixture"; source.projectPath = "."; flow.nodes[6] = source; flow.edges[0] = {6,3}; flow.edges.push_back({1,6});
        const Json message = {{"message", "Choose one"}, {"details", {1,2}}};
        runner.hostHandler = [message](Runner *, Id, const Node &, const QString &, Runner::HostCompletion done) { done(message,{}); };
        QSignalSpy done(&runner,&Runner::finished), closed(&runner,&Runner::userInputClosed); std::vector<UserInputRequest> requests;
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){requests.push_back(request);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(1)); QTest::qWait(30); QVERIFY(done.isEmpty()); QVERIFY(runner.running());
        QVERIFY(requests[0].message == message); QCOMPARE(requests[0].choices,flow.nodes[3].userChoices); QVERIFY(runner.results().at(3).state == State::Running);
        QVERIFY(!runner.submitUserInput("stale",text,choice)); QVERIFY(!runner.submitUserInput(requests[0].id,text,2));
        QVERIFY(runner.submitUserInput(requests[0].id,text,choice)); QVERIFY(!runner.submitUserInput(requests[0].id,"duplicate",choice));
        QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QCOMPARE(closed.count(),1); QVERIFY(runner.results().at(2).output == Json(text.toUtf8().toStdString()));
        QCOMPARE(runner.results().at(choice == 0 ? 4 : 5).executions,quint64(1)); QVERIFY(runner.results().at(choice == 0 ? 5 : 4).state == State::Skipped);
    }
    void loopAndHistories() {
        QTemporaryDir directory; auto flow = Flow::initial(); flow.nodes[3] = inputNode(); flow.edges = {{1,3},{3,3,0,0},{3,2,1,0}};
        Runner runner; std::vector<UserInputRequest> requests; std::vector<NodeExecution> history; QSignalSpy done(&runner,&Runner::finished);
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){requests.push_back(request);});
        connect(&runner,&Runner::executionChanged,&runner,[&](const NodeExecution &entry){if(entry.node==3 && entry.state==State::Succeeded) history.push_back(entry);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(1)); QVERIFY(runner.submitUserInput(requests[0].id,"First answer",0));
        QTRY_COMPARE(requests.size(),size_t(2)); QVERIFY(requests[0].id != requests[1].id); QVERIFY(requests[1].message == "First answer"); QVERIFY(done.isEmpty());
        QVERIFY(!runner.submitUserInput(requests[0].id,"stale",1)); QVERIFY(runner.submitUserInput(requests[1].id,"Final answer",1)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool());
        QCOMPARE(history.size(),size_t(2)); QVERIFY(history[0].id != history[1].id); QCOMPARE(history[0].executionNumber,quint64(1)); QCOMPARE(history[1].executionNumber,quint64(2)); QVERIFY(history[0].output == "First answer"); QVERIFY(history[1].output == "Final answer");
    }
    void parallelAndJoin() {
        QTemporaryDir directory; auto flow = Flow::initial();
        for (Id id : {3u,4u}) { flow.nodes[id] = inputNode(id); flow.nodes[id].userChoices = {"Send"}; }
        Node join; join.id = 5; join.kind = Kind::All; flow.nodes[5] = join; flow.edges = {{1,3},{1,4},{3,5,0,0},{4,5,0,1},{5,2}};
        Runner runner; QSignalSpy done(&runner,&Runner::finished); std::vector<UserInputRequest> requests;
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){requests.push_back(request);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(2)); QVERIFY(runner.submitUserInput(requests[1].id,"second",0)); QTest::qWait(20); QVERIFY(done.isEmpty());
        QVERIFY(runner.submitUserInput(requests[0].id,"first",0)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(runner.results().at(2).output == Json::array({"first","second"}));
    }
    void queuedEvents() {
        QTemporaryDir directory; auto flow = Flow::initial(); flow.nodes[3] = inputNode(); flow.nodes[3].userChoices = {"Send"};
        Node pass; pass.id = 4; pass.kind = Kind::Branch; flow.nodes[4] = pass; flow.edges = {{1,3},{1,4},{4,3},{3,2}};
        Runner runner; QSignalSpy done(&runner,&Runner::finished); std::vector<UserInputRequest> requests;
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){requests.push_back(request);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(1)); QTest::qWait(20); QCOMPARE(requests.size(),size_t(1));
        QVERIFY(runner.submitUserInput(requests[0].id,"one",0)); QTRY_COMPARE(requests.size(),size_t(2)); QVERIFY(runner.submitUserInput(requests[1].id,"two",0));
        QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QCOMPARE(runner.results().at(3).executions,quint64(2)); QVERIFY(runner.results().at(2).output == "two");
    }
    void cancelAndRestart() {
        QTemporaryDir directory; auto flow = branching(); Runner runner; QSignalSpy done(&runner,&Runner::finished), closed(&runner,&Runner::userInputClosed); std::vector<UserInputRequest> requests;
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){requests.push_back(request);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(1)); const auto old = requests[0].id;
        QVERIFY(runner.submitUserInput(old,"cancelled",0)); runner.cancel(); QCOMPARE(done.count(),1); QCOMPARE(closed.count(),1);
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requests.size(),size_t(2)); QVERIFY(!runner.submitUserInput(old,"stale",0)); QVERIFY(runner.submitUserInput(requests[1].id,"new",1));
        QTRY_COMPARE(done.count(),2); QVERIFY(done[1][0].toBool()); QVERIFY(runner.results().at(2).output == "new");
    }
    void childProjectRouting() {
        QTemporaryDir directory; Coordinator host; Runner parent; auto target = branching(); auto flow = Flow::initial();
        Node run; run.id = 3; run.kind = Kind::FlowRun; run.targetFlow = "child"; run.projectPath = "."; flow.nodes[3] = run;
        auto wait = run; wait.id = 4; wait.kind = Kind::FlowWait; flow.nodes[4] = wait; flow.edges = {{1,3},{3,4},{4,2}};
        host.flows = {{"parent",flow},{"child",target}}; host.projects = {{"root","parent",directory.path()},{"child-project","child",directory.path()}};
        QString project; UserInputRequest pending; QSignalSpy done(&parent,&Runner::finished), closed(&host,&Coordinator::userInputClosed);
        connect(&host,&Coordinator::userInputRequested,&host,[&](QString id,const UserInputRequest &request){project=id;pending=request;});
        QVERIFY(host.start(host.projects[0],&parent)); QTRY_VERIFY(!pending.id.isEmpty()); QCOMPARE(project,QString("child-project")); QVERIFY(host.running());
        QVERIFY(host.submitUserInput(pending.id,"child answer",1)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QCOMPARE(closed.count(),1); QVERIFY(parent.results().at(2).output == "child answer"); QVERIFY(!host.running());
    }
    void failureClosesRequests() {
        QTemporaryDir directory; auto flow = Flow::initial(); flow.nodes[3] = inputNode(); flow.nodes[3].userChoices = {"Send"};
        Node missingHost; missingHost.id = 4; missingHost.kind = Kind::FlowWait; missingHost.targetFlow = "target"; missingHost.projectPath = "."; flow.nodes[4] = missingHost;
        flow.edges = {{1,3},{1,4},{3,2},{4,2}}; Runner runner; QSignalSpy done(&runner,&Runner::finished), closed(&runner,&Runner::userInputClosed); QString pending;
        connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){pending=request.id;});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(done.count(),1); QVERIFY(!done[0][0].toBool()); QVERIFY(!pending.isEmpty()); QCOMPARE(closed.count(),1);
        QVERIFY(runner.results().at(3).state == State::Cancelled); QVERIFY(!runner.submitUserInput(pending,"late",0));
    }
};
QTEST_GUILESS_MAIN(UserInputTest)
#include "userinput_test.moc"
