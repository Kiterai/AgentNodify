#include "gui_test.h"
#include "window.h"
#include "nodeform.h"
#include <QtTest>
#include <QComboBox>
#include <QDir>
#include <QGraphicsProxyWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSignalSpy>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QTextBlock>
#include <QTextLayout>
#include <QtNodes/NodeStyle>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

namespace an {
using namespace QtNodes;
void GuiTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("AgentNodify GUI tests");
}
void GuiTest::previewHeightAndClick() {
    TextPreview preview; preview.setFixedWidth(280); preview.show();
    preview.setPlainText("short"); QCOMPARE(preview.visibleLines(), 1);
    const auto oneLine = preview.height();
    preview.setPlainText(QStringList{"a", "b", "c"}.join('\n'));
    QCOMPARE(preview.visibleLines(), 3); QVERIFY(preview.height() > oneLine);
    preview.setPlainText(QString("line\n").repeated(40)); QCOMPARE(preview.visibleLines(), 8);
    const auto capped = preview.height();
    preview.setPlainText(QString("line\n").repeated(100)); QCOMPARE(preview.height(), capped);
    preview.setPlainText(QString("long wrapped text ").repeated(100)); QCOMPARE(preview.visibleLines(), 8);
    preview.setPlainText(""); QCOMPARE(preview.visibleLines(), 1); QCOMPARE(preview.height(), oneLine);
    QSignalSpy edits(&preview, &TextPreview::editRequested);
    QTest::mouseClick(preview.viewport(), Qt::LeftButton); QCOMPARE(edits.count(), 1);
    QTest::keyClick(&preview, Qt::Key_Return); QCOMPARE(edits.count(), 2);
    QVERIFY(preview.isReadOnly());
}
void GuiTest::embeddedParameters() {
    Graph graph; BasicGraphicsScene scene(graph); GraphicsView view(&scene); view.resize(900, 700); view.show();
    graph.replace(Flow::initial()); auto id = graph.addNode("agent");
    auto *form = graph.form(id); QVERIFY(form); QVERIFY(form->graphicsProxyWidget());
    QCOMPARE(form->graphicsProxyWidget()->parentItem(), static_cast<QGraphicsItem *>(scene.nodeGraphicsObject(id)));
    auto *title = form->findChild<QLineEdit *>("nodeTitle"); QVERIFY(title);
    graph.setNodeData(id, NodeRole::Position, QPointF(200, 100));
    title->setFocus(); title->selectAll(); QTest::keyClicks(title, "Review");
    QCOMPARE(graph.flow.nodes.at(id).title, QString("Review"));
    QCOMPARE(graph.flow.nodes.at(id).position, QPointF(200, 100));
    QCOMPARE(graph.form(id), form); QCOMPARE(graph.nodeData(id, NodeRole::Caption).toString(), QString("Review"));
    auto *agent = form->findChild<QComboBox *>("nodeAgent"), *model = form->findChild<QComboBox *>("nodeModel");
    QVERIFY(agent && model); agent->setCurrentText("OpenCode"); model->setCurrentText("openai/gpt-6-luna");
    QCOMPARE(graph.flow.nodes.at(id).agent, QString("OpenCode")); QCOMPARE(graph.flow.nodes.at(id).model, QString("openai/gpt-6-luna"));
    const auto before = graph.nodeData(id, NodeRole::Size).toSize();
    auto node = graph.flow.nodes.at(id); node.prompt = QString("line\n").repeated(40); graph.updateNode(node);
    auto *preview = form->findChild<TextPreview *>("promptPreview"); QVERIFY(preview);
    QCOMPARE(preview->visibleLines(), 8);
    QTRY_VERIFY(graph.nodeData(id, NodeRole::Size).toSize().height() > before.height());
    const auto expanded = graph.nodeData(id, NodeRole::Size).toSize().height();
    node.prompt = "short"; graph.updateNode(node);
    QTRY_COMPARE(preview->visibleLines(), 1);
    QTRY_VERIFY(graph.nodeData(id, NodeRole::Size).toSize().height() < expanded);
    QTRY_COMPARE(form->height(), form->sizeHint().height());
    QTRY_VERIFY2(preview->verticalScrollBar()->maximum() == 0,
        qPrintable(QString("height=%1 viewport=%2 frame=%3 font=%4 margin=%5 block=%6").arg(preview->height())
            .arg(preview->viewport()->height()).arg(preview->frameWidth()).arg(preview->fontMetrics().height())
            .arg(preview->document()->documentMargin()).arg(preview->document()->firstBlock().layout()->boundingRect().height())));
}
void GuiTest::branchAndInputForms() {
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial());
    auto branch = graph.addNode("branch"); auto *form = graph.form(branch); QVERIFY(form);
    auto *variable = form->findChild<QLineEdit *>("ruleVariable"); QVERIFY(variable);
    variable->selectAll(); QTest::keyClicks(variable, "status");
    QCOMPARE(graph.flow.nodes.at(branch).rules.at(0).variable, QString("status"));
    auto *value = form->findChild<QLineEdit *>("ruleValue"); value->selectAll(); QTest::keyClicks(value, "done");
    form->findChild<QComboBox *>("ruleOperator")->setCurrentIndex(1);
    QVERIFY(!graph.flow.nodes.at(branch).rules[0].equals); QCOMPARE(graph.flow.nodes.at(branch).rules[0].value, QString("done"));
    QTest::mouseClick(form->findChild<QPushButton *>("addRule"), Qt::LeftButton);
    QCOMPARE(graph.flow.nodes.at(branch).rules.size(), size_t(2)); QCOMPARE(graph.flow.outputCount(graph.flow.nodes.at(branch)), 3u);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::mouseClick(form->findChildren<QPushButton *>("removeRule").last(), Qt::LeftButton);
    QCOMPARE(graph.flow.nodes.at(branch).rules.size(), size_t(1)); QCOMPARE(graph.form(branch), form);
    auto join = graph.addNode("and"); graph.addConnection({1, 0, join, 0});
    auto *joinForm = graph.form(join); auto *inputs = joinForm->findChild<QSpinBox *>("nodeInputs"); QVERIFY(inputs);
    inputs->setValue(3); QCOMPARE(graph.flow.nodes.at(join).inputs, 3u);
    QVERIFY(graph.allConnectionIds(join).empty()); QCOMPARE(graph.form(join), joinForm);
}
void GuiTest::terminalAppearance() {
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial());
    for (auto id : {1u, 2u}) {
        QVERIFY(!graph.nodeData(id, NodeRole::CaptionVisible).toBool());
        QVERIFY(!graph.nodeData(id, NodeRole::Widget).value<QWidget *>());
        auto port = id == 1 ? PortType::Out : PortType::In;
        QCOMPARE(graph.portData(id, port, 0, PortRole::Caption).toString(), kindName(graph.flow.nodes.at(id).kind));
        NodeStyle style(QJsonObject::fromVariantMap(graph.nodeData(id, NodeRole::Style).toMap()));
        QCOMPARE(style.GradientColor0, QColor("#efc65e")); QCOMPARE(style.GradientColor3, QColor("#efc65e"));
        QVERIFY(graph.nodeData(id, NodeRole::Size).toSize().height() < 70);
    }
}
void GuiTest::editorAndFlowRail() {
    Window window(true); window.show();
    auto *palette = window.findChild<QWidget *>("nodePalette"); QVERIFY(palette);
    QVERIFY(palette->findChildren<QLineEdit *>().isEmpty());
    for (auto *button : palette->findChildren<QPushButton *>()) QVERIFY(!button->text().contains("接続"));
    for (auto kind : {Kind::Start, Kind::End, Kind::Agent, Kind::And, Kind::Branch}) {
        auto *button = palette->findChild<QPushButton *>("addNode_" + kindKey(kind)); QVERIFY(button); QCOMPARE(button->toolTip(), nodeDescription(kind));
    }
    for (auto *label : window.findChildren<QLabel *>()) QVERIFY(label->text() != "AN");
    QTest::mouseClick(palette->findChild<QPushButton *>("addNode_agent"), Qt::LeftButton);
    Graph *graph = nullptr;
    for (auto *candidate : window.findChildren<Graph *>()) if (!candidate->readOnly) graph = candidate;
    QVERIFY(graph); Id agent = 0; for (auto &[id, node] : graph->flow.nodes) if (node.kind == Kind::Agent) agent = id;
    QVERIFY(agent != 0);
    auto *form = graph->form(agent);
    auto *editorPane = window.findChild<QWidget *>("textEditorPane");
    auto *editor = window.findChild<QPlainTextEdit *>("parameterEditor"); QVERIFY(form && editorPane && editor);
    auto *view = window.findChild<GraphicsView *>("flowEditorView"); QVERIFY(view);
    auto *scene = qobject_cast<BasicGraphicsScene *>(view->scene()); QVERIFY(scene);
    QCOMPARE(scene->nodeGraphicsObject(agent)->toolTip(), nodeDescription(Kind::Agent));
    QVERIFY(!editorPane->isVisible());
    auto *preview = form->findChild<TextPreview *>("promptPreview"); QTest::mouseClick(preview->viewport(), Qt::LeftButton);
    QTRY_VERIFY(editorPane->isVisible());
    editor->setPlainText(QString("content\n").repeated(20));
    QCOMPARE(graph->flow.nodes.at(agent).prompt, editor->toPlainText()); QCOMPARE(preview->toPlainText(), editor->toPlainText());
    QCOMPARE(preview->visibleLines(), 8);
    editor->setPlainText("Say hello"); QTRY_COMPARE(preview->visibleLines(), 1);
    auto *schema = form->findChild<TextPreview *>("schemaPreview"); QTest::mouseClick(schema->viewport(), Qt::LeftButton);
    editor->setPlainText("{\n  \"type\": \"string\"\n}");
    QCOMPARE(graph->flow.nodes.at(agent).schema, editor->toPlainText()); QCOMPARE(schema->toPlainText(), editor->toPlainText());
    auto *rail = window.findChild<QWidget *>("flowRail"); QVERIFY(rail);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    auto *add = rail->findChild<QToolButton *>("addFlowTile"); auto tiles = rail->findChildren<QToolButton *>("flowTile");
    QVERIFY(add && tiles.size() == 1); QCOMPARE(add->parentWidget(), tiles[0]->parentWidget());
    QCOMPARE(add->size(), QSize(56, 56)); QCOMPARE(tiles[0]->size(), add->size()); QVERIFY(tiles[0]->isChecked());
    QVERIFY(window.styleSheet().contains(":hover")); QVERIFY(window.styleSheet().contains("border-radius:14px"));
    // Save a local offscreen snapshot for visual review without touching user data.
    graph->addConnection({1, 0, agent, 0}); graph->addConnection({agent, 0, 2, 0});
    graph->setNodeData(1, NodeRole::Position, QPointF(0, 80)); graph->setNodeData(agent, NodeRole::Position, QPointF(180, 20)); graph->setNodeData(2, NodeRole::Position, QPointF(720, 80));
    QCoreApplication::processEvents(); view->setupScale(1.0); view->centerScene(); QCoreApplication::processEvents();
    QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-editor.png")));
    QTest::mouseClick(form->findChild<QLineEdit *>("nodeTitle"), Qt::LeftButton);
    QTRY_VERIFY(!editorPane->isVisible());
    QTest::mouseClick(add, Qt::LeftButton);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(rail->findChildren<QToolButton *>("flowTile").size(), 2);
    QVERIFY(!editorPane->isVisible());
}
void GuiTest::widgetLifetime() {
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial());
    auto id = graph.addNode("agent"); QPointer<NodeForm> form = graph.form(id); QVERIFY(form);
    graph.replace(Flow::initial()); QVERIFY(form.isNull());
    id = graph.addNode("agent"); form = graph.form(id);
    graph.deleteNode(id); QVERIFY(form.isNull());
    graph.readOnly = true; graph.replace(Flow::initial());
    QVERIFY(graph.nodeData(1, NodeRole::Widget).value<QWidget *>() == nullptr);
}
}
QTEST_MAIN(an::GuiTest)
