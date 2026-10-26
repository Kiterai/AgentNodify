#include "flow.h"
#include "core_test.h"
#include "runner.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QCoreApplication>

using namespace an;
    Flow CoreTest::simple(const QString &agent) {
        auto f = Flow::initial(); Node n; n.id = 3; n.kind = Kind::Agent; n.title = "test"; n.agent = agent; n.prompt = "Say hello";
        f.nodes[3] = n; f.edges = {{1, 3}, {3, 2}}; return f;
    }
    QString CoreTest::schema() { return R"({"type":"object","properties":{"result":{"type":"string"}},"required":["result"],"additionalProperties":false})"; }
    AgentConfig CoreTest::config() {
        auto path = QCoreApplication::applicationDirPath() + "/fake_agent.exe";
#ifndef Q_OS_WIN
        path = QCoreApplication::applicationDirPath() + "/fake_agent";
#endif
        return {path, path, path, 10};
    }
    void CoreTest::variables() {
        Json j = {{"a", "b"}, {"hoge", {{"foo", "bar"}}}, {"foobar", {"baz", "qux"}}};
        QCOMPARE(interpolate("${a} ${hoge.foo} ${foobar[1]}", j), QString("b bar qux"));
        QCOMPARE(interpolate("${[1].a}", Json::array({nullptr, {{"a", "joined"}}})), QString("joined"));
        QVERIFY_EXCEPTION_THROWN(interpolate("${missing}", j), std::exception);
        QVERIFY_EXCEPTION_THROWN(resolve(j, "foobar[8]"), std::exception);
        QVERIFY_EXCEPTION_THROWN(resolve(j, "a..b"), std::exception);
        QVERIFY_EXCEPTION_THROWN(resolve(j, "foobar[-1]"), std::exception);
        QCOMPARE(interpolate("${a}", {{"a", "${do_not_expand}"}}), QString("${do_not_expand}"));
    }
    void CoreTest::schemas() {
        try {
        checkSchema(parseJson(schema()));
        checkSchema(Json(true));
        QVERIFY_EXCEPTION_THROWN(checkSchema(Json{{"$schema", "https://json-schema.org/draft/2020-12/schema"}}), std::exception);
        QVERIFY_EXCEPTION_THROWN(checkSchema(Json{{"type", "banana"}}), std::exception);
        QVERIFY_EXCEPTION_THROWN(checkSchema(Json{{"required", 42}}), std::exception);
        QVERIFY_EXCEPTION_THROWN(checkSchema(Json{{"properties", {{"x", {{"type", "banana"}}}}}}), std::exception);
        validateOutput(parseJson(schema()), {{"result", "ok"}});
        QVERIFY_EXCEPTION_THROWN(validateOutput(parseJson(schema()), {{"result", 1}}), std::exception);
        auto ref = parseJson(R"({"definitions":{"r":{"type":"object","properties":{"x":{"type":"string"}}}},"$ref":"#/definitions/r"})");
        QCOMPARE(schemaAt(ref, "x").at("type").get<std::string>(), std::string("string"));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void CoreTest::roundtrip() {
        QTemporaryDir dir; auto f = simple(); f.name = "日本語フロー"; f.nodes[3].prompt = "複数行\nプロンプト";
        f.save(dir.filePath("flow.json")); auto copy = Flow::load(dir.filePath("flow.json"));
        QVERIFY(f.toJson() == copy.toJson()); QVERIFY(copy.validate().isEmpty());
        auto j = f.toJson(); j["nodes"].push_back(j["nodes"][0]);
        QVERIFY_EXCEPTION_THROWN(Flow::fromJson(j), std::exception);
    }
    void CoreTest::invalidFlows() {
        auto f = simple(); QVERIFY(f.validate().isEmpty());
        f.edges.push_back({3, 3}); QVERIFY(f.validate().isEmpty());
        f = simple(); f.nodes[3].schema = "{"; QVERIFY(!f.validate().isEmpty());
        f = simple(); f.nodes[3].schema = R"({"type":"not-a-type"})"; QVERIFY(!f.validate().isEmpty());
        f = simple(); f.nodes[3].prompt = "${missing}"; QVERIFY(f.validate().join('\n').contains("変数"));
        f = simple(); f.edges.push_back({1, 2}); QVERIFY(f.validate().isEmpty());
        f.edges.push_back({1, 2}); QVERIFY(!f.validate().isEmpty());
        f = simple(); f.edges[0].input = 22; QVERIFY(!f.validate().isEmpty());
        f = simple(); f.edges[0].from = 999; QVERIFY(!f.validate().isEmpty());
        f = simple(); f.nodes.erase(1); QVERIFY(!f.validate().isEmpty());
        f = simple(); f.edges.clear(); QVERIFY(!f.validate().isEmpty());
        f = simple(); f.nodes[3].prompt.clear(); QVERIFY(!f.validate().isEmpty());
    }
    void CoreTest::schemaReferences() {
        auto f = simple(); f.nodes[3].schema = schema();
        Node n = f.nodes[3]; n.id = 4; n.prompt = "${result}"; f.nodes[4] = n;
        f.edges = {{1, 3}, {3, 4}, {4, 2}}; QVERIFY(f.validate().isEmpty());
        f.nodes[4].prompt = "${absent}"; QVERIFY(!f.validate().isEmpty());
        f.nodes[4].prompt = "${result.x}"; QVERIFY(!f.validate().isEmpty());
    }
    void CoreTest::protocols() {
        auto open = decodeAgentOutput("OpenCode", "{\"type\":\"text\",\"part\":{\"text\":\"hello\"}}\n", false);
        QVERIFY(open == "hello");
        auto claude = decodeAgentOutput("Claude Code", "{\"type\":\"result\",\"structured_output\":{\"a\":1}}\n", true);
        QVERIFY(claude == Json({{"a", 1}}));
        auto codex = decodeAgentOutput("Codex", "{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"hello\"}}\n", false);
        QVERIFY(codex == "hello");
        QVERIFY_EXCEPTION_THROWN(decodeAgentOutput("OpenCode", "not json", false), std::exception);
        QVERIFY_EXCEPTION_THROWN(decodeAgentOutput("OpenCode", "{\"type\":\"error\"}", false), std::exception);
    }
    void CoreTest::cli_data() {
        QTest::addColumn<QString>("agent"); QTest::addColumn<bool>("structured");
        QTest::addColumn<bool>("standalone");
        for (QString agent : {"Codex", "Claude Code", "OpenCode"}) {
            QTest::newRow(qPrintable(agent + " text")) << agent << false << false;
            QTest::newRow(qPrintable(agent + " JSON")) << agent << true << false;
        }
        QTest::newRow("OpenCode standalone text") << QString("OpenCode") << false << true;
        QTest::newRow("OpenCode standalone JSON") << QString("OpenCode") << true << true;
    }
    void CoreTest::cli() {
        QFETCH(QString, agent); QFETCH(bool, structured);
        QFETCH(bool, standalone);
        QTemporaryDir dir; QFile marker(dir.filePath("marker.txt")); QVERIFY(marker.open(QIODevice::WriteOnly)); marker.close();
        auto f = simple(agent); f.nodes[3].prompt = "CHECK_CWD quote \" and $() 日本語\nhello";
        if (structured) f.nodes[3].schema = schema();
        auto cfg = config(); cfg.opencodeStandalone = standalone;
        Runner r; QSignalSpy spy(&r, &Runner::finished); QVERIFY(r.start(f, dir.path(), cfg));
        QVERIFY(spy.wait(15000)); QVERIFY2(spy[0][0].toBool(), qPrintable(spy[0][1].toString()));
        QVERIFY(r.results().at(2).output == (structured ? Json{{"result", "ok"}} : Json("hello")));
    }
    void CoreTest::branchAndJoin() {
        auto f = simple(); f.nodes[3].schema = schema();
        Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"result", "ok", true}, {"result", "ok", true}}; f.nodes[4] = branch;
        f.edges = {{1, 3}, {3, 4}, {4, 2, 0, 0}, {4, 2, 1, 0}, {4, 2, 2, 0}};
        QVERIFY2(f.validate().isEmpty(), qPrintable(f.validate().join('\n')));
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(spy[0][0].toBool()); QVERIFY(r.results().at(2).output == Json({{"result", "ok"}}));
        QCOMPARE(r.results().at(2).executions, quint64(1));
        f.nodes[4].rules = {{"result", "no", true}, {"result", "ok", false}};
        spy.clear(); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(r.results().at(2).output == Json({{"result", "ok"}}));
        QCOMPARE(r.results().at(2).executions, quint64(1));
    }
    void CoreTest::parallelJoin() {
        auto f = simple(); auto second = f.nodes[3]; second.id = 4; second.prompt = "RESPONSE_TWO"; f.nodes[4] = second;
        Node join; join.id = 5; join.kind = Kind::And; f.nodes[5] = join;
        f.edges = {{1, 3}, {1, 4}, {4, 5, 0, 0}, {3, 5, 0, 1}, {5, 2}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(spy[0][0].toBool()); QVERIFY(r.results().at(2).output == Json::array({"two", "hello"}));
    }
    void CoreTest::inactiveAgent() {
        auto f = simple(); f.nodes[3].schema = schema();
        Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"result", "ok", true}}; f.nodes[4] = branch;
        Node yes = f.nodes[3]; yes.id = 5; yes.schema.clear(); yes.prompt = "EXPECT_OK=${result}"; f.nodes[5] = yes;
        Node no = yes; no.id = 6; no.prompt = "FAIL"; f.nodes[6] = no;
        f.edges = {{1, 3}, {3, 4}, {4, 5, 0, 0}, {4, 6, 1, 0}, {5, 2}, {6, 2}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        QVERIFY2(r.start(f, dir.path(), config()), qPrintable(f.validate().join('\n'))); QVERIFY(spy.wait(15000));
        QVERIFY(spy[0][0].toBool()); QVERIFY(r.results().at(6).state == State::Skipped);
        QVERIFY(r.results().at(6).log.isEmpty()); QVERIFY(r.results().at(2).output == "hello");
    }
    void CoreTest::examples() {
        for (QString name : {"codex", "claude", "opencode", "branch", "loop"}) {
            auto f = Flow::load(QString(AGENTNODIFY_SOURCE_DIR) + "/examples/" + name + ".flow.json");
            QVERIFY2(f.validate().isEmpty(), qPrintable(f.validate().join('\n')));
        }
    }
    void CoreTest::loopExecution() {
        auto f = simple();
        f.nodes[3].schema = R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"]})";
        f.nodes[3].prompt = "COUNT_TO_3";
        Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"count", "3", false}};
        f.nodes[4] = branch; f.edges = {{1, 3}, {3, 4}, {4, 3, 0, 0}, {4, 2, 1, 0}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        QVERIFY2(r.start(f, dir.path(), config()), qPrintable(f.validate().join('\n'))); QVERIFY(spy.wait(15000));
        QVERIFY2(spy[0][0].toBool(), qPrintable(spy[0][1].toString()));
        QCOMPARE(r.results().at(3).executions, quint64(3));
        QCOMPARE(r.results().at(4).executions, quint64(3));
        QVERIFY(r.results().at(2).output == Json({{"count", 3}}));
        // A second run must not reuse scheduled events or old FIFO contents.
        QFile::remove(dir.filePath("counter.txt")); spy.clear();
        QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QCOMPARE(r.results().at(3).executions, quint64(3));
    }
    void CoreTest::repeatedAnd() {
        auto f = simple();
        f.nodes[3].schema = R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"]})";
        f.nodes[3].prompt = "COUNT_TO_3";
        Node branch; branch.id = 4; branch.kind = Kind::Branch; branch.rules = {{"count", "3", false}}; f.nodes[4] = branch;
        Node a = f.nodes[3]; a.id = 5; f.nodes[5] = a;
        Node b = a; b.id = 6; b.schema.clear(); b.prompt = "SHORT_DELAY"; f.nodes[6] = b;
        Node join; join.id = 7; join.kind = Kind::And; f.nodes[7] = join;
        Node next = a; next.id = 8; next.prompt = "PASS_COUNT=${[0].count}"; f.nodes[8] = next;
        f.edges = {{1, 3}, {3, 4}, {4, 5, 0, 0}, {4, 6, 0, 0}, {4, 2, 1, 0}, {5, 7, 0, 0}, {6, 7, 0, 1}, {7, 8}, {8, 4}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished), fired(&r, &Runner::fired);
        QVERIFY2(r.start(f, dir.path(), config()), qPrintable(f.validate().join('\n'))); QVERIFY(spy.wait(15000));
        QVERIFY2(spy[0][0].toBool(), qPrintable(spy[0][1].toString()));
        QCOMPARE(r.results().at(7).executions, quint64(2));
        QCOMPARE(r.results().at(6).executions, quint64(2));
        std::vector<Json> outputs;
        for (auto &args : fired) if (args[0].toUInt() == 7) outputs.push_back(parseJson(args[2].toString()));
        QVERIFY(outputs == std::vector<Json>({Json::array({Json{{"count", 2}}, "hello"}), Json::array({Json{{"count", 3}}, "hello"})}));
        QVERIFY(r.results().at(2).output == Json({{"count", 3}}));
    }
    void CoreTest::queuedActivations() {
        auto f = simple();
        Node second = f.nodes[3]; second.id = 4; second.prompt = "RESPONSE_TWO"; f.nodes[4] = second;
        Node consumer = second; consumer.id = 5; consumer.prompt = "SHORT_DELAY"; f.nodes[5] = consumer;
        f.edges = {{1, 3}, {1, 4}, {3, 5}, {4, 5}, {5, 2}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000)); QVERIFY(spy[0][0].toBool());
        QCOMPARE(r.results().at(5).executions, quint64(2)); QCOMPARE(r.results().at(2).executions, quint64(2));
        // Two inputs to each AND port, queued before the slower input arrives.
        Node join; join.id = 6; join.kind = Kind::And; f.nodes[6] = join;
        f.edges = {{1, 3}, {1, 4}, {3, 5}, {4, 5}, {3, 6, 0, 0}, {4, 6, 0, 0}, {5, 6, 0, 1}, {6, 2}};
        spy.clear(); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000)); QVERIFY(spy[0][0].toBool());
        QCOMPARE(r.results().at(6).executions, quint64(2)); QCOMPARE(r.results().at(2).executions, quint64(2));
    }
    void CoreTest::incompleteAnd() {
        auto f = simple(); Node join; join.id = 4; join.kind = Kind::And; f.nodes[4] = join;
        // Input 1 fires once. The join's own feedback must NOT reuse it.
        f.edges = {{1, 3}, {3, 4, 0, 0}, {1, 4, 0, 1}, {4, 4, 0, 0}, {4, 2}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(!spy[0][0].toBool()); QVERIFY(spy[0][1].toString().contains("AND"));
        QCOMPARE(r.results().at(4).executions, quint64(1));
    }
    void CoreTest::synchronousLoopCancellation() {
        auto f = Flow::initial(); Node b; b.id = 3; b.kind = Kind::Branch; f.nodes[3] = b;
        f.edges = {{1, 3}, {3, 3}, {3, 2}};
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        QVERIFY(r.start(f, dir.path(), config()));
        QTimer::singleShot(20, &r, &Runner::cancel);
        QVERIFY(spy.wait(3000)); QVERIFY(!spy[0][0].toBool()); QVERIFY(!r.running());
        QVERIFY(r.results().at(3).executions > 1);
    }
    void CoreTest::loopValidation() {
        auto f = simple(); f.nodes[3].schema = schema();
        Node n = f.nodes[3]; n.id = 4; n.prompt = "${result}"; f.nodes[4] = n;
        f.edges = {{1, 3}, {3, 4}, {4, 4}, {4, 2}};
        QVERIFY(f.validate().isEmpty());
        f.nodes[4].prompt = "${absent}"; QVERIFY(!f.validate().isEmpty());
        // Both the initial predecessor and the back edge must define variables.
        f.nodes[4].prompt = "${result}"; f.nodes[3].schema = R"({"type":"object","properties":{"different":{"type":"string"}}})";
        QVERIFY(!f.validate().isEmpty());
    }
    void CoreTest::failuresAndCancellation() {
        QTemporaryDir dir; auto f = simple(); f.nodes[3].prompt = "FAIL";
        Runner r; QSignalSpy spy(&r, &Runner::finished); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(!spy[0][0].toBool()); QVERIFY(r.results().at(3).state == State::Failed);
        spy.clear(); f.nodes[3].prompt = "SLEEP"; QVERIFY(r.start(f, dir.path(), config()));
        QTRY_VERIFY(r.results().at(3).state == State::Running); r.cancel();
        QCOMPARE(spy.count(), 1); QVERIFY(r.results().at(3).state == State::Cancelled); QVERIFY(!r.running());
        spy.clear(); auto c = config(); c.timeoutSeconds = 1; QVERIFY(r.start(f, dir.path(), c)); QVERIFY(spy.wait(10000));
        QVERIFY(!spy[0][0].toBool()); QVERIFY(spy[0][1].toString().contains("タイムアウト"));
        spy.clear(); f.nodes[3].prompt = "BAD_SCHEMA"; f.nodes[3].schema = schema(); QVERIFY(r.start(f, dir.path(), config())); QVERIFY(spy.wait(15000));
        QVERIFY(!spy[0][0].toBool());
        spy.clear(); c.codex = dir.filePath("missing.exe"); f = simple(); QVERIFY(r.start(f, dir.path(), c)); QVERIFY(spy.wait(10000)); QVERIFY(!spy[0][0].toBool());
    }
    void CoreTest::rejectBeforeExecution() {
        QTemporaryDir dir; Runner r; QSignalSpy spy(&r, &Runner::finished);
        auto f = simple(); f.edges.push_back({3, 3, 0, 9}); QVERIFY(!r.start(f, dir.path(), config())); QCOMPARE(spy.count(), 1);
        QVERIFY(!r.running());
        spy.clear(); QVERIFY(!r.start(simple(), dir.filePath("nonexistent"), config())); QCOMPARE(spy.count(), 1);
    }
QTEST_GUILESS_MAIN(CoreTest)
