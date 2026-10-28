#include "core/schedule.h"
#include "core/runner.h"
#include "core/coordinator.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTimeZone>
#include <QElapsedTimer>
using namespace an;
namespace {
QDateTime utc(QString value) { return QDateTime::fromString(value,Qt::ISODateWithMs); }
Node timerNode(Kind kind,Id id=3) { Node node; node.id=id; node.kind=kind; node.waitDuration=1; return node; }
}
class TimingTest : public QObject {
    Q_OBJECT
private slots:
    void definitions() {
        auto flow=Flow::initial(false); QCOMPARE(flow.nodes.size(),size_t(1)); QVERIFY(flow.validate().isEmpty());
        auto delay=timerNode(Kind::Delay); flow.nodes[3]=delay; flow.edges={{1,3},{3,3}}; QVERIFY(flow.validate().isEmpty());
        auto schedule=timerNode(Kind::Schedule,4); schedule.scheduleMonth="1,2"; schedule.scheduleDay="*/2"; schedule.scheduleHour="3,12"; schedule.scheduleMinute="*/5"; schedule.scheduleWeekdays={1,3,5};
        flow.nodes[4]=schedule; flow.edges={{1,3},{3,4},{4,3}}; QVERIFY(flow.validate().isEmpty()); QVERIFY(Flow::fromJson(flow.toJson()).toJson()==flow.toJson());
        auto legacy=flow.toJson(); for(auto &node:legacy["nodes"]) {node.erase("waitDuration");node.erase("waitUnit");node.erase("scheduleMonth");node.erase("scheduleWeekdays");}
        const auto restored=Flow::fromJson(legacy); QCOMPARE(restored.nodes.at(3).waitDuration,1); QCOMPARE(restored.nodes.at(3).waitUnit,QString("seconds")); QCOMPARE(restored.nodes.at(4).scheduleMonth,QString("*")); QVERIFY(restored.nodes.at(4).scheduleWeekdays.empty());
        auto fractional=flow.toJson();for(auto &node:fractional["nodes"])if(node["kind"]=="delay")node["waitDuration"]=0.5;QVERIFY_THROWS_EXCEPTION(std::exception,Flow::fromJson(fractional));
        Node end; end.kind=Kind::End; end.id=2; flow.nodes[2]=end; end.id=5; flow.nodes[5]=end; QVERIFY(!flow.validate().isEmpty());
    }
    void units() {
        auto node=timerNode(Kind::Delay); node.waitDuration=2;
        for(const auto &[unit,expected]:std::vector<std::pair<QString,qint64>>{{"seconds",2000},{"minutes",120000},{"hours",7200000},{"days",172800000}}) {node.waitUnit=unit; QCOMPARE(waitMilliseconds(node),expected);}
        node.waitUnit="seconds"; node.waitDuration=1; QCOMPARE(waitMilliseconds(node),qint64(1000)); node.waitDuration=0; QCOMPARE(waitMilliseconds(node),qint64(0));
        node.waitDuration=-1; QVERIFY_THROWS_EXCEPTION(std::exception,waitMilliseconds(node)); node.waitDuration=1; node.waitUnit="invalid"; QVERIFY_THROWS_EXCEPTION(std::exception,waitMilliseconds(node));
    }
    void invalidSchedules_data() {
        QTest::addColumn<QString>("month"); QTest::addColumn<QString>("day"); QTest::addColumn<QString>("hour"); QTest::addColumn<QString>("minute");
        auto row=[](const char *name,QString month,QString day,QString hour,QString minute){QTest::newRow(name)<<month<<day<<hour<<minute;};
        row("month bounds","13","*","*","*"); row("day zero","*","0","*","*"); row("hour bounds","*","*","24","*"); row("minute bounds","*","*","*","60");
        row("zero step","*","*","*","*/0"); row("negative","*","*","*","-1"); row("empty token","*","*","*","1,,2"); row("unsupported range","*","*","*","1-5");
        row("no month multiple","*/13","*","*","*"); row("impossible date","2","30","*","*");
    }
    void invalidSchedules() {
        QFETCH(QString,month); QFETCH(QString,day); QFETCH(QString,hour); QFETCH(QString,minute);
        auto node=timerNode(Kind::Schedule); node.scheduleMonth=month; node.scheduleDay=day; node.scheduleHour=hour; node.scheduleMinute=minute;
        bool rejected=false; try {Schedule(node).next(utc("2026-01-01T00:00:00Z"));}catch(const std::exception&){rejected=true;} QVERIFY(rejected);
    }
    void nextMatches() {
        auto node=timerNode(Kind::Schedule); node.scheduleMonth=" "; node.scheduleMinute="";
        QCOMPARE(Schedule(node).next(utc("2026-01-01T00:00:00Z")),utc("2026-01-01T00:01:00Z"));
        node.scheduleMinute="0, 15,30,45"; QCOMPARE(Schedule(node).next(utc("2026-01-01T00:15:01Z")),utc("2026-01-01T00:30:00Z"));
        node.scheduleMinute="*/15"; QCOMPARE(Schedule(node).next(utc("2026-01-01T00:59:59Z")),utc("2026-01-01T01:00:00Z"));
        node.scheduleMonth="*/2"; node.scheduleDay="*/2"; node.scheduleHour="0"; node.scheduleMinute="0";
        QCOMPARE(Schedule(node).next(utc("2026-01-31T00:00:00Z")),utc("2026-02-02T00:00:00Z"));
        node.scheduleMonth="2"; node.scheduleDay="29"; QCOMPARE(Schedule(node).next(utc("2025-01-01T00:00:00Z")),utc("2028-02-29T00:00:00Z"));
        node.scheduleMonth="*"; node.scheduleDay="*"; node.scheduleHour="9"; node.scheduleMinute="30"; node.scheduleWeekdays={1,3};
        QCOMPARE(Schedule(node).next(utc("2026-10-27T10:00:00Z")),utc("2026-10-28T09:30:00Z"));
        node.scheduleDay="1"; auto next=Schedule(node).next(utc("2026-10-01T00:00:00Z")); QCOMPARE(next.date().day(),1); QVERIFY(next.date().dayOfWeek()==1 || next.date().dayOfWeek()==3);
        node.scheduleWeekdays={0}; QVERIFY_THROWS_EXCEPTION(std::exception,(void)Schedule(node));
    }
    void daylightSaving() {
        const QTimeZone zone("America/New_York"); if(!zone.isValid())QSKIP("IANA timezone not available");
        auto node=timerNode(Kind::Schedule); node.scheduleHour="1"; node.scheduleMinute="30";
        auto after=utc("2026-11-01T05:40:00Z").toTimeZone(zone); QCOMPARE(Schedule(node).next(after).toUTC(),utc("2026-11-01T06:30:00Z"));
        node.scheduleHour="2"; after=utc("2026-03-08T06:59:00Z").toTimeZone(zone);
        const auto next=Schedule(node).next(after); QCOMPARE(next.date(),QDate(2026,3,9)); QCOMPARE(next.time(),QTime(2,30));
    }
    void delayFifo() {
        QTemporaryDir directory; auto flow=Flow::initial();
        for(Id id:{3u,4u}){Node input;input.id=id;input.kind=Kind::UserInput;flow.nodes[id]=input;} flow.nodes[5]=timerNode(Kind::Delay,5);
        flow.edges={{1,3},{1,4},{3,5},{4,5},{5,2}}; Runner runner; QSignalSpy done(&runner,&Runner::finished); QSignalSpy requested(&runner,&Runner::userInputRequested); std::vector<Json> outputs;
        connect(&runner,&Runner::executionChanged,&runner,[&](const NodeExecution &entry){if(entry.node==5 && entry.state==State::Succeeded)outputs.push_back(entry.output);});
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(requested.count(),2);
        QElapsedTimer elapsed;elapsed.start(); for(int i=0;i<2;++i){const auto request=qvariant_cast<UserInputRequest>(requested[i][0]); QVERIFY(runner.submitUserInput(request.id,i?"B":"A",0));}
        QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QCOMPARE(outputs.size(),size_t(2)); QVERIFY(outputs[0]=="A"); QVERIFY(outputs[1]=="B"); QVERIFY(elapsed.elapsed()>=1900);
    }
    void endlessAndCancel() {
        QTemporaryDir directory; auto flow=Flow::initial(false); flow.nodes[3]=timerNode(Kind::Delay); flow.nodes[3].waitDuration=0; flow.edges={{1,3},{3,3}};
        Runner runner; QSignalSpy done(&runner,&Runner::finished); QVERIFY(runner.start(flow,directory.path(),{})); QTRY_VERIFY(runner.results().at(3).executions>=3); QVERIFY(done.isEmpty()); QVERIFY(runner.running());
        runner.cancel(); QCOMPARE(done.count(),1); QVERIFY(!done[0][0].toBool()); const auto count=runner.results().at(3).executions; QTest::qWait(60); QCOMPARE(runner.results().at(3).executions,count);
        done.clear(); QVERIFY(runner.start(Flow::initial(false),directory.path(),{})); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool());
        flow.nodes[3].waitDuration=1000000; flow.nodes[3].waitUnit="days"; done.clear(); QVERIFY(runner.start(flow,directory.path(),{})); QTRY_VERIFY(runner.results().at(3).state==State::Running); QTest::qWait(20); QVERIFY(done.isEmpty()); runner.cancel(); QCOMPARE(done.count(),1);
    }
    void scheduledRuntime() {
        QTemporaryDir directory; auto flow=Flow::initial(false); flow.nodes[3]=timerNode(Kind::Schedule); flow.edges={{1,3},{3,3}};
        auto now=utc("2026-10-01T00:00:59Z"); Runner runner(nullptr,[&]{return now;}); QSignalSpy done(&runner,&Runner::finished); std::vector<QDateTime> targets;
        connect(&runner,&Runner::executionChanged,&runner,[&](const NodeExecution &entry){
            if(entry.node!=3 || entry.state!=State::Running)return;
            if(entry.executionNumber==3){runner.cancel();return;}
            if(entry.log.startsWith("次の予定:")){const auto target=QDateTime::fromString(entry.log.mid(QString("次の予定: ").size()).trimmed(),Qt::ISODateWithMs);targets.push_back(target);now=target;}
        });
        QVERIFY(runner.start(flow,directory.path(),{})); QTRY_COMPARE(done.count(),1); QVERIFY(!done[0][0].toBool()); QCOMPARE(targets.size(),size_t(2)); QCOMPARE(targets[0],utc("2026-10-01T00:01:00Z")); QCOMPARE(targets[1],utc("2026-10-01T00:02:00Z"));
        Runner pending(nullptr,[]{return utc("2026-10-01T00:00:00Z");}); QSignalSpy pendingDone(&pending,&Runner::finished); QVERIFY(pending.start(flow,directory.path(),{})); QTRY_VERIFY(pending.results().at(3).state==State::Running); QVERIFY(pendingDone.isEmpty());pending.cancel(); QCOMPARE(pendingDone.count(),1); QTest::qWait(20); QCOMPARE(pendingDone.count(),1);
    }
    void noEndCoordinator() {
        QTemporaryDir directory; Coordinator host; const auto child=directory.filePath("child.json"), parent=directory.filePath("parent.json"); host.flows[child]=Flow::initial(false); host.projects.push_back({"child",child,directory.path()});
        QSignalSpy done(&host,&Coordinator::sessionFinished); QVERIFY(host.start(host.projects[0])); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][1].toBool());
        auto flow=Flow::initial(); Node wait;wait.id=3;wait.kind=Kind::FlowWait;wait.targetFlow=child;wait.projectPath=directory.path();flow.nodes[3]=wait;flow.edges={{1,3},{3,2}};host.flows[parent]=flow;host.projects.push_back({"parent",parent,directory.path()});
        Runner runner;QSignalSpy finished(&runner,&Runner::finished);QVERIFY(host.start(host.projects[1],&runner));QTRY_COMPARE(finished.count(),1);QVERIFY(finished[0][0].toBool());QVERIFY(runner.results().at(2).output.is_null());
    }
};
QTEST_GUILESS_MAIN(TimingTest)
#include "timing_test.moc"
