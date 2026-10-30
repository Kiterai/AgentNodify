#include "agent_registry_test.h"
#include "core/agents/definition.h"
#include "core/progress.h"
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

using namespace an;

    void AgentRegistryTest::registrationIsComplete() {
        for(const auto *name : {"Codex","Claude Code","OpenCode"}) QVERIFY(agentNames().contains(name));
        QSet<QString> names,keys;
        for(const auto &definition : agentDefinitions()) {
            QVERIFY(definition.name); QVERIFY(definition.configKey); QVERIFY(definition.defaultProgram);
            QVERIFY(!names.contains(definition.name)); names.insert(definition.name);
            QVERIFY(!keys.contains(definition.configKey)); keys.insert(definition.configKey);
            QCOMPARE(findAgentDefinition(definition.name),&definition);
            QVERIFY(definition.prepare); QVERIFY(definition.outputEvent); QVERIFY(definition.sessionField);
            QVERIFY(definition.catalog); QVERIFY(definition.progress);
            QVERIFY(definition.catalog()); QVERIFY(definition.progress());
            QVERIFY(QRegularExpression(definition.sessionPattern).isValid());
        }
    }
    void AgentRegistryTest::configurationRemainsCompatible() {
        AgentConfig config; config.codex="codex-native.exe";config.claude="claude-native.exe";config.opencode="open-native.exe";
        const auto saved=agentConfigJson(config);
        QCOMPARE(saved,Json({{"codex","codex-native.exe"},{"claude","claude-native.exe"},{"opencode","open-native.exe"}}));
        const auto restored=agentConfigFromJson(saved);
        for(const auto &definition : agentDefinitions()) QCOMPARE(agentExecutable(definition,restored),agentExecutable(definition,config));
        AgentDefinition extra;extra.name="Future Agent";extra.configKey="future";extra.defaultProgram="future-cli";
        QCOMPARE(agentExecutable(extra,config),QString("future-cli"));
        setAgentExecutable(extra,config,"custom-future-cli"); QCOMPARE(agentExecutable(extra,config),QString("custom-future-cli"));
        QCOMPARE(config.codex,QString("codex-native.exe"));
        const auto migrated=agentConfigFromJson(Json{{"opencode","opencode"}});
#ifdef Q_OS_WIN
        QCOMPARE(migrated.opencode,QString("opencode-cli"));
#else
        QCOMPARE(migrated.opencode,QString("opencode"));
#endif
        QCOMPARE(agentConfigFromJson(Json::object()).codex,AgentConfig{}.codex);
    }
    void AgentRegistryTest::launchUsesRegisteredImplementation() {
        AgentConfig config{"codex-native","claude-native","open-native"};
        for(const auto &definition : agentDefinitions()) {
            Node node;node.agent=definition.name;node.model="fixture/model";node.effort="high";
            node.schema=R"({"type":"object","properties":{},"required":[],"additionalProperties":false})";
            ProcessJob job;ProcessLaunch launch;
            const auto prompt=prepareAgentLaunch({node,config,"project-directory","prompt",""},job,launch);
            QCOMPARE(launch.program,agentExecutable(definition,config)); QVERIFY(job.temp); QVERIFY(job.temp->isValid());
            QVERIFY(!launch.args.isEmpty());
            if(definition.promptOnStdin) QCOMPARE(launch.standardInput,prompt.toUtf8());
            else {QVERIFY(launch.standardInput.isEmpty());QCOMPARE(launch.args.back(),prompt);}
        }
        Node unknown;unknown.agent="unknown-agent";ProcessJob job;ProcessLaunch launch;
        QVERIFY(!findAgentDefinition(unknown.agent));
        QVERIFY_THROWS_EXCEPTION(std::exception,prepareAgentLaunch({unknown,config,{},"prompt",{}},job,launch));
        QVERIFY_THROWS_EXCEPTION(std::exception,decodeAgentOutput(unknown.agent,{},false));
        QVERIFY_THROWS_EXCEPTION(std::exception,decodeAgentSessionId(unknown.agent,{}));
    }
    void AgentRegistryTest::catalogProtocolInstancesAreIndependent() {
        const auto &definition=agentDefinition("Codex");
        auto first=definition.catalog(),second=definition.catalog();
        QCOMPARE(first->started().messages,second->started().messages);
        const auto response=Json{{"id",1},{"result",Json::object()}};
        QCOMPARE(first->receive(response).messages.size(),size_t(2));
        // Initialization state belongs to each request, never a static provider.
        QCOMPARE(second->receive(response).messages.size(),size_t(2));
        const auto action=first->receive(Json::parse(R"({"id":2,"result":{"data":[{"model":"fixture"}]}})"));
        QVERIFY(action.finished); QCOMPARE(first->models().size(),size_t(1)); QVERIFY(second->models().empty());
    }
    void AgentRegistryTest::progressStateAndResetRemainLocal() {
        const QString start="{\"type\":\"stream_event\",\"event\":{\"type\":\"message_start\",\"message\":{\"id\":\"same\"}}}\n";
        const QString block="{\"type\":\"stream_event\",\"event\":{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"one\"}}}\n";
        const QString delta="{\"type\":\"stream_event\",\"event\":{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" two\"}}}\n";
        ProgressParser first,second;first.append(start+block);second.append(start+block);first.append(delta);
        QCOMPARE(first.items().front().content,Json("one two"));QCOMPARE(second.items().front().content,Json("one"));
        first=ProgressParser();first.append(start+block);QCOMPARE(first.items().front().content,Json("one"));
    }
    void AgentRegistryTest::implementationBoundariesStayLocal() {
        const QDir source(QStringLiteral(AGENTNODIFY_SOURCE_DIR)+"/src");
        for(const auto *relative : {"core/runner.cpp","core/modelcatalog.cpp","core/progress.cpp","nodes/agent/core/process.inc","nodes/agent/core/definition.inc","nodes/agent/ui/form.inc","ui/window.cpp"}) {
            QFile file(source.filePath(relative));QVERIFY(file.open(QIODevice::ReadOnly));const auto text=QString::fromUtf8(file.readAll());
            for(const auto *name : {"strings::Codex","strings::ClaudeCode","strings::OpenCode","strings::ClaudeTool","strings::Opencode"})
                QVERIFY2(!text.contains(name),qPrintable(QString(relative)+": "+name));
        }
        const QRegularExpression includes(R"inc(^\s*#include\s+"([^"]+)")inc",QRegularExpression::MultilineOption);
        QDirIterator files(source.filePath("core/agents"),{"*.cpp","*.h"},QDir::Files,QDirIterator::Subdirectories);
        while(files.hasNext()) {
            const auto path=files.next();const auto relative=source.relativeFilePath(path);
            QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto text=QString::fromUtf8(file.readAll());
            auto matches=includes.globalMatch(text);
            while(matches.hasNext()) {
                const auto include=matches.next().captured(1);
                QFileInfo target(QDir(QFileInfo(path).path()).filePath(include));
                if(!target.exists()) target=QFileInfo(source.filePath(include));
                if(!target.exists()) continue;
                const auto imported=source.relativeFilePath(target.canonicalFilePath());
                if(!imported.startsWith("core/agents/") || imported.section('/',2,2)=="common" || !imported.section('/',3,3).size()) continue;
                QVERIFY2(relative=="core/agents/registry.cpp" || relative.section('/',2,2)==imported.section('/',2,2),qPrintable(relative+" -> "+imported));
            }
        }
    }
QTEST_GUILESS_MAIN(AgentRegistryTest)
