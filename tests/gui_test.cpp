#include "gui_test.h"
#include "window.h"
#include "nodeform.h"
#include "theme.h"
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
#include <QMenu>
#include <QMessageBox>
#include <QUuid>
#include <QStyledItemDelegate>
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
    for (int lines = 2; lines <= 8; ++lines) {
        node.prompt = QString("line\n").repeated(lines - 1) + "last";
        graph.updateNode(node); QCoreApplication::processEvents();
        QCOMPARE(preview->visibleLines(), lines);
        QVERIFY(!preview->verticalScrollBar()->isVisible());
        QCOMPARE(preview->verticalScrollBar()->maximum(), 0);
    }
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
    QVERIFY(add->menu()); QCOMPARE(add->menu()->actions().size(), 2);
    QCOMPARE(add->menu()->actions()[0]->text(), QString("新規フロー"));
    QCOMPARE(add->menu()->actions()[1]->text(), QString("既存フローの追加"));
    QCOMPARE(tiles[0]->contextMenuPolicy(), Qt::CustomContextMenu);
    add->menu()->actions()[0]->trigger();
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
void GuiTest::comboPopupColors() {
    Node agent; agent.kind = Kind::Agent; NodeForm agentForm(agent);
    Node branch; branch.kind = Kind::Branch; branch.rules.push_back({"status", "ok", true}); NodeForm branchForm(branch);
    QComboBox connection; connection.addItems({"始端", "エージェント", "終端"}); styleComboBoxPopup(&connection);
    QList<QComboBox *> combos = agentForm.findChildren<QComboBox *>();
    combos.append(branchForm.findChildren<QComboBox *>()); combos.append(&connection);
    QCOMPARE(combos.size(), 5);
    for (auto *combo : combos) {
        auto *view = combo->view(); view->ensurePolished();
        QVERIFY(qobject_cast<QStyledItemDelegate *>(view->itemDelegate()));
        for (auto group : {QPalette::Active, QPalette::Inactive}) {
            QCOMPARE(view->palette().color(group, QPalette::Text), QColor("#e6edf8"));
            QCOMPARE(view->palette().color(group, QPalette::HighlightedText), QColor("#ffffff"));
            QCOMPARE(view->palette().color(group, QPalette::Base), QColor("#17202e"));
        }
    }
    connection.resize(240, 35); connection.show(); connection.showPopup(); QCoreApplication::processEvents();
    QVERIFY(connection.view()->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-combo-popup.png")));
    const auto second = connection.model()->index(1, 0);
    QTest::mouseClick(connection.view()->viewport(), Qt::LeftButton, Qt::NoModifier, connection.view()->visualRect(second).center());
    QCOMPARE(connection.currentIndex(), 1);
    connection.hidePopup();
}
void GuiTest::workSelection() {
    Window window(true); window.show();
    auto id = window.editGraph_->addNode("agent");
    auto node = window.editGraph_->flow.nodes.at(id); node.title = "Work node"; node.effort = "medium";
    node.prompt = "line one\nline two"; window.editGraph_->updateNode(node);
    window.tabs_->setCurrentIndex(1); QCoreApplication::processEvents();
    auto *form = window.workGraph_->form(id); QVERIFY(form);
    QCOMPARE(form->node().prompt, node.prompt); QCOMPARE(form->node().effort, node.effort);
    auto *item = window.workScene_->nodeGraphicsObject(id); QVERIFY(item);
    QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsSelectable));
    QVERIFY(!item->flags().testFlag(QGraphicsItem::ItemIsMovable));
    window.workView_->centerScene(); QCoreApplication::processEvents();
    const auto point = window.workView_->mapFromScene(item->mapToScene(QPointF(item->boundingRect().width() / 2, 12)));
    QTest::mouseClick(window.workView_->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(window.nodeStatus_->text().contains("Work node"));
    window.selectWorkNode(0);
    QTest::mouseClick(form->findChild<QLineEdit *>("nodeTitle"), Qt::LeftButton);
    QTRY_VERIFY(window.nodeStatus_->text().contains("Work node"));
    QTest::keyClicks(form->findChild<QLineEdit *>("nodeTitle"), "MUTATION");
    QCOMPARE(form->node().title, node.title); QCOMPARE(window.workGraph_->flow.nodes.at(id).title, node.title);
    window.projects_.push_back({"test", ".", window.documents_[0].path, {}}); window.project_ = 0;
    auto &result = window.projects_[0].results[id]; result.state = State::Running; result.executions = 1; result.log = "progress 日本語";
    window.selectWorkNode(id); QCOMPARE(window.log_->toPlainText(), result.log); QVERIFY(window.nodeStatus_->text().contains("実行中"));
    result.state = State::Succeeded; result.output = "created file"; window.selectWorkNode(id);
    QVERIFY(window.output_->toPlainText().contains("created file"));
    auto *effort = window.editGraph_->form(id)->findChild<QComboBox *>("nodeEffort");
    effort->setCurrentIndex(effort->findData("high"));
    QCOMPARE(window.editGraph_->flow.nodes.at(id).effort, QString("high"));
    QCOMPARE(Flow::fromJson(window.editGraph_->flow.toJson()).nodes.at(id).effort, QString("high"));
}
void GuiTest::flowRemoval() {
    Window window(true);
    const auto root = QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("removal-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root + "/flows")); window.dataDir_ = root;
    window.documents_.clear(); window.current_ = -1; window.newFlow();
    const auto internal = window.documents_[0].path; window.documents_[0].flow.save(internal);
    auto confirm = [&] { QTimer::singleShot(0, &window, [] { if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click(); }); };
    confirm(); window.removeFlow(0); QVERIFY(!QFileInfo::exists(internal));
    const auto external = root + "/external.json"; Flow::initial().save(external);
    window.documents_.push_back({Flow::initial(), external, false});
    confirm(); window.removeFlow(1); QVERIFY(QFileInfo::exists(external));
    QVERIFY(QFile::remove(external)); QVERIFY(QDir(root).rmdir("flows")); QVERIFY(QDir().rmdir(root));
}
}
QTEST_MAIN(an::GuiTest)
