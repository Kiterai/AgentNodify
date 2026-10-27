#include "catalog_test.h"
#include "modelcatalog.h"
#include "graph.h"
#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalSpy>
#include <QPushButton>
#include <QLabel>

namespace an {
namespace {
AgentConfig fixtureConfig() {
    auto path = QCoreApplication::applicationDirPath() + "/fake_catalog";
#ifdef Q_OS_WIN
    path += ".exe";
#endif
    AgentConfig config; config.codex = config.claude = config.opencode = path; return config;
}
}
void CatalogTest::init() { qunsetenv("AN_CATALOG_FIXTURE_MODE"); }
void CatalogTest::cleanup() { qunsetenv("AN_CATALOG_FIXTURE_MODE"); }
void CatalogTest::protocols_data() {
    QTest::addColumn<QString>("agent");
    for (QString agent : {"Codex", "Claude Code", "OpenCode"}) QTest::newRow(qPrintable(agent)) << agent;
}
void CatalogTest::protocols() {
    QFETCH(QString, agent);
    ModelCatalog catalog; catalog.setConfig(fixtureConfig()); catalog.request(agent);
    QTRY_VERIFY(catalog.entry(agent).state != CatalogState::Loading);
    const auto entry = catalog.entry(agent); QVERIFY2(entry.state == CatalogState::Ready, qPrintable(entry.error));
    QCOMPARE(entry.models.size(), size_t(2));
    if (agent == "Codex") { QCOMPARE(entry.models[0].id, QString("code-alpha")); QCOMPARE(entry.models[1].id, QString("code-luna")); }
    else if (agent == "Claude Code") { QCOMPARE(entry.models[1].id, QString("haiku")); QVERIFY(entry.models[1].description.contains("claude-haiku-5-5")); }
    else QCOMPARE(entry.models[1].id, QString("openai/code-luna"));
    if (agent == "Codex") QCOMPARE(entry.models[0].efforts, (QStringList{"medium", "high"}));
    if (agent == "Claude Code") QCOMPARE(entry.models[1].efforts, (QStringList{"low", "medium"}));
}
void CatalogTest::failures_data() {
    QTest::addColumn<QString>("agent"); QTest::addColumn<QByteArray>("mode");
    for (QString agent : {"Codex", "Claude Code", "OpenCode"}) for (QByteArray mode : {"error", "malformed", "large", "hang"})
        QTest::newRow(qPrintable(agent + " " + mode)) << agent << mode;
}
void CatalogTest::failures() {
    QFETCH(QString, agent); QFETCH(QByteArray, mode); qputenv("AN_CATALOG_FIXTURE_MODE", mode);
    ModelCatalog catalog(nullptr, mode == "hang" ? 120 : 5000); catalog.setConfig(fixtureConfig()); catalog.request(agent);
    QTRY_VERIFY(catalog.entry(agent).state == CatalogState::Error); QVERIFY(!catalog.entry(agent).error.isEmpty());
}
void CatalogTest::cacheAndRefresh() {
    ModelCatalog catalog; catalog.setConfig(fixtureConfig()); QSignalSpy changes(&catalog, &ModelCatalog::changed);
    catalog.request("Codex"); catalog.request("Codex");
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Ready); QCOMPARE(changes.count(), 2);
    catalog.request("Codex"); QCOMPARE(changes.count(), 2);
    qputenv("AN_CATALOG_FIXTURE_MODE", "error"); catalog.request("Codex", true);
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Error); QCOMPARE(catalog.entry("Codex").models.size(), size_t(2));
    auto count = changes.count(); catalog.request("Codex"); QCOMPARE(changes.count(), count);
    qunsetenv("AN_CATALOG_FIXTURE_MODE"); catalog.request("Codex", true);
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Ready);
}
void CatalogTest::pathChange() {
    ModelCatalog catalog; auto config = fixtureConfig(); catalog.setConfig(config); catalog.request("Codex");
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Ready);
    config.codex = QCoreApplication::applicationDirPath() + "/missing-catalog-cli.exe";
    catalog.setConfig(config); QCOMPARE(catalog.entry("Codex").models.size(), size_t(0)); catalog.request("Codex");
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Error);
    // Old in-flight results must not overwrite a new configuration.
    qputenv("AN_CATALOG_FIXTURE_MODE", "hang"); catalog.setConfig(fixtureConfig()); catalog.request("Codex");
    catalog.setConfig(config); catalog.request("Codex");
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Error);
}
void CatalogTest::emptyList() {
    qputenv("AN_CATALOG_FIXTURE_MODE", "empty"); ModelCatalog catalog; catalog.setConfig(fixtureConfig()); catalog.refreshAll();
    for (QString agent : {"Codex", "Claude Code", "OpenCode"}) {
        QTRY_VERIFY(catalog.entry(agent).state == CatalogState::Ready); QVERIFY(catalog.entry(agent).models.empty());
    }
}
void CatalogTest::paginationLoop() {
    qputenv("AN_CATALOG_FIXTURE_MODE", "cursor-loop"); ModelCatalog catalog; catalog.setConfig(fixtureConfig()); catalog.request("Codex");
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Error); QVERIFY(catalog.entry("Codex").error.contains("ページ"));
}
void CatalogTest::manualValuePreserved() {
    ModelCatalog catalog; catalog.setConfig(fixtureConfig()); Graph graph; graph.modelCatalog = &catalog;
    const auto id = graph.addNode("agent"); auto node = graph.flow.nodes.at(id); node.model = "custom-id"; graph.updateNode(node);
    auto *form = graph.form(id); auto *combo = form->findChild<QComboBox *>("nodeModel"); QVERIFY(combo);
    QSignalSpy edited(&graph, &Graph::edited);
    QTRY_VERIFY(catalog.entry("Codex").state == CatalogState::Ready);
    auto *status=form->findChild<QLabel *>("modelCatalogStatus"); QVERIFY(status); QVERIFY(status->text().isEmpty()); QVERIFY(status->isHidden());
    QCOMPARE(combo->currentData().toString(), QString("custom-id")); QCOMPARE(graph.flow.nodes.at(id).model, QString("custom-id")); QCOMPARE(edited.count(), 0);
    const auto alpha = combo->findData("code-alpha"); QVERIFY(alpha >= 0); combo->setCurrentIndex(alpha);
    QCOMPARE(graph.flow.nodes.at(id).model, QString("code-alpha")); QCOMPARE(combo->currentText(), combo->itemText(alpha)); QVERIFY(!combo->currentText().contains(" · "));
    auto *effort = form->findChild<QComboBox *>("nodeEffort"); QVERIFY(effort);
    QCOMPARE(effort->count(), 3); QVERIFY(effort->findData("low") < 0); QVERIFY(effort->findData("high") >= 0);
    QVERIFY(!combo->isEditable()); QVERIFY(!combo->lineEdit());
    combo->setCurrentText("new-manual-id"); QCOMPARE(graph.flow.nodes.at(id).model, QString("code-alpha"));
    QVERIFY(combo->findData("") < 0); QVERIFY(combo->findText("CLI既定") < 0);
    auto *agent = form->findChild<QComboBox *>("nodeAgent"); agent->setCurrentText("Claude Code");
    QTRY_VERIFY(catalog.entry("Claude Code").state == CatalogState::Ready);
    QVERIFY(combo->findData("haiku") >= 0); QVERIFY(combo->findData("code-alpha") < 0);
    QCOMPARE(graph.flow.nodes.at(id).model, QString());
    qputenv("AN_CATALOG_FIXTURE_MODE", "error");
    auto *refresh = form->findChild<QPushButton *>("refreshModels"); QVERIFY(refresh->isEnabled()); refresh->click();
    QTRY_VERIFY(catalog.entry("Claude Code").state == CatalogState::Error);
    QVERIFY(combo->isEnabled() && !combo->isEditable()); QVERIFY(combo->findData("haiku") >= 0);
}
}
QTEST_MAIN(an::CatalogTest)
