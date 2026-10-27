#include "gui_test.h"
#include "window.h"
#include "nodeform.h"
#include "theme.h"
#include "projectopener.h"
#include "history.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QComboBox>
#include <QFileDialog>
#include <QScopeGuard>
#include <QTimer>
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
#include <QContextMenuEvent>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QStatusBar>
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
    QVERIFY(agent && model); agent->setCurrentText("OpenCode");
    QVERIFY(!model->isEditable()); QCOMPARE(model->currentIndex(), -1);
    auto imported = graph.flow.nodes.at(id); imported.model = "openai/gpt-6-luna"; graph.updateNode(imported);
    QCOMPARE(model->currentData().toString(), imported.model);
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
    for (auto kind : {Kind::Start, Kind::End, Kind::Agent, Kind::Script, Kind::Shell, Kind::And, Kind::Branch}) {
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
    auto nodeStyle = [&](Id nodeId) { return NodeStyle(QJsonObject::fromVariantMap(window.workGraph_->nodeData(nodeId, NodeRole::Style).toMap())); };
    auto selectedStyle = nodeStyle(id);
    QCOMPARE(selectedStyle.NormalBoundaryColor, QColor("#ffffff"));
    QCOMPARE(selectedStyle.SelectedBoundaryColor, QColor("#ffffff"));
    QVERIFY(selectedStyle.PenWidth >= 4.0f); QVERIFY(selectedStyle.HoveredPenWidth >= 4.0f);
    window.workScene_->clearSelection(); window.log_->setFocus(); QCoreApplication::processEvents();
    QVERIFY(!item->isSelected()); QCOMPARE(nodeStyle(id).NormalBoundaryColor, QColor("#ffffff"));
    QTest::mouseClick(window.workView_->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QCOMPARE(window.workSelected_, id); QCOMPARE(nodeStyle(id).NormalBoundaryColor, QColor("#ffffff"));
    auto *end = window.workScene_->nodeGraphicsObject(2);
    const auto endPoint = window.workView_->mapFromScene(end->mapToScene(end->boundingRect().center()));
    QTest::mouseClick(window.workView_->viewport(), Qt::LeftButton, Qt::NoModifier, endPoint);
    QCOMPARE(window.workSelected_, 2u); QCOMPARE(nodeStyle(2).NormalBoundaryColor, QColor("#ffffff"));
    QVERIFY(nodeStyle(id).NormalBoundaryColor != QColor("#ffffff")); QVERIFY(nodeStyle(id).PenWidth < nodeStyle(2).PenWidth);
    QCOMPARE(nodeStyle(2).GradientColor0, QColor("#efc65e")); // Keep the terminal's yellow fill.
    window.workGraph_->states[2] = State::Failed; emit window.workGraph_->nodeUpdated(2);
    window.historyList_->setFocus(); QCoreApplication::processEvents();
    QCOMPARE(nodeStyle(2).NormalBoundaryColor, QColor("#ffffff"));
    window.tabs_->setCurrentIndex(0); window.tabs_->setCurrentIndex(1);
    QCOMPARE(window.workSelected_, 2u); QCOMPARE(nodeStyle(2).NormalBoundaryColor, QColor("#ffffff"));
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
void GuiTest::nodeContextAndMenuSpacing() {
    Window window(true); window.show();
    auto id = window.editGraph_->addNode("agent"); window.editGraph_->addConnection({1, 0, id, 0});
    window.editView_->centerScene(); QCoreApplication::processEvents();
    auto openNodeMenu = [&](Id node, bool enabled) {
        auto *item = window.editScene_->nodeGraphicsObject(node); QVERIFY(item);
        auto point = window.editView_->mapFromScene(item->mapToScene(QPointF(item->boundingRect().width() / 2, 12)));
        bool shown = false;
        QTimer::singleShot(0, &window, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu) return;
            shown = menu->objectName() == "nodeContextMenu";
            const auto text = menu->actions()[0]->text(); const bool active = menu->actions()[0]->isEnabled();
            if (enabled) menu->actions()[0]->trigger(); menu->close();
            QCOMPARE(text, QString("削除")); QCOMPARE(active, enabled);
        });
        QContextMenuEvent event(QContextMenuEvent::Mouse, point, window.editView_->viewport()->mapToGlobal(point));
        QApplication::sendEvent(window.editView_->viewport(), &event); QVERIFY(shown);
    };
    openNodeMenu(id, true); QVERIFY(!window.editGraph_->nodeExists(id)); QVERIFY(window.editGraph_->flow.edges.empty());
    openNodeMenu(1, false); QVERIFY(window.editGraph_->nodeExists(1));
    auto *file = window.findChild<QMenu *>("fileMenu"); QVERIFY(file);
    file->popup(window.mapToGlobal(QPoint(10, 30))); QCoreApplication::processEvents();
    int widest = 0;
    for (auto *action : file->actions()) if (action->shortcut() == QKeySequence::SaveAs) {
        const auto rect = file->actionGeometry(action);
        widest = rect.width();
        const auto textWidth = file->fontMetrics().horizontalAdvance(action->text());
        const auto keyWidth = file->fontMetrics().horizontalAdvance(action->shortcut().toString(QKeySequence::NativeText));
        QVERIFY(rect.width() - textWidth - keyWidth >= 60);
    }
    QVERIFY(widest > 0); QVERIFY(file->width() - widest < 30);
    QVERIFY(file->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-file-menu.png"))); file->close();
}
void GuiTest::embeddedComboCursor() {
    Window window(true); window.show(); const auto id = window.editGraph_->addNode("agent");
    window.editView_->centerScene(); QCoreApplication::processEvents();
    auto *form = window.editGraph_->form(id);
    for (auto *combo : form->findChildren<QComboBox *>()) {
        QCOMPARE(combo->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(combo->view()->viewport()->cursor().shape(), Qt::ArrowCursor);
        if (combo->lineEdit()) QCOMPARE(combo->lineEdit()->cursor().shape(), Qt::ArrowCursor);
        auto point = window.editView_->mapFromScene(form->graphicsProxyWidget()->mapToScene(combo->mapTo(form, combo->rect().center())));
        QTest::mouseMove(window.editView_->viewport(), QPoint(5, 5));
        QTest::mouseMove(window.editView_->viewport(), point); QCoreApplication::processEvents();
        QCOMPARE(window.editView_->viewport()->cursor().shape(), Qt::ArrowCursor);
    }
}
void GuiTest::projectOpeners() {
    const auto detected = detectProjectOpeners();
    QSet<QString> labels;
    for (const auto &opener : detected) {
        QVERIFY(QFileInfo(opener.program).isFile()); QVERIFY(!labels.contains(opener.label)); labels.insert(opener.label);
        qInfo().noquote() << "Detected:" << opener.label << opener.program;
    }
    QVERIFY(detected.size() <= 6);
    const auto path = QDir::current().absoluteFilePath("プロジェクト & (sample)");
    for (const auto app : {ProjectApp::Explorer, ProjectApp::CommandPrompt, ProjectApp::PowerShell, ProjectApp::GitBash, ProjectApp::Code, ProjectApp::CodeInsiders}) {
        const ProjectOpener opener{app, "", ""}; const auto args = opener.arguments(path);
        const auto native = QDir::toNativeSeparators(path);
        if (app == ProjectApp::Explorer) QCOMPARE(args, QStringList{native});
        else if (app == ProjectApp::GitBash) QCOMPARE(args, QStringList{"--cd=" + native});
        else if (app == ProjectApp::Code || app == ProjectApp::CodeInsiders) QCOMPARE(args, (QStringList{"--new-window", native}));
        else QVERIFY(!args.join(' ').contains(path)); // Shells use QProcess cwd, never an interpolated cd command.
        QString error; QVERIFY(!openProjectDirectory(opener, path, &error)); QVERIFY(!error.isEmpty());
    }
    Window window(true); window.projects_.push_back({"test", QDir::currentPath(), window.documents_[0].path, {}}); window.refreshProjects();
    auto *more = window.projectList_->findChild<QToolButton *>("projectOpenMenu"); QVERIFY(more && more->menu());
    auto *remove = window.projectList_->findChild<QToolButton *>("removeProject"); QVERIFY(remove);
    QCOMPARE(more->text(), QString("…"));
    QVERIFY(more->parentWidget()->layout()->indexOf(more) < more->parentWidget()->layout()->indexOf(remove));
    more->menu()->aboutToShow(); QCOMPARE(more->menu()->actions().size(), qsizetype(detected.size()));
    for (int i = 0; i < more->menu()->actions().size(); ++i) QCOMPARE(more->menu()->actions()[i]->text(), detected[size_t(i)].label);
    window.show(); window.tabs_->setCurrentIndex(1); QCoreApplication::processEvents();
    more->menu()->popup(more->mapToGlobal(QPoint(0, more->height()))); QCoreApplication::processEvents();
    QVERIFY(more->menu()->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-project-open-menu.png"))); more->menu()->close();
    const auto id = window.editGraph_->addNode("agent"); auto node = window.editGraph_->flow.nodes.at(id); node.prompt = "test"; window.editGraph_->updateNode(node);
    window.editGraph_->addConnection({1, 0, id, 0}); window.editGraph_->addConnection({id, 0, 2, 0});
    window.config_.codex = "nonexistent-test-agent"; // Never run a real CLI from this regression test.
    window.executeFlow(); QVERIFY(!window.runner_->running()); QVERIFY(window.statusBar()->currentMessage().contains("モデルを選択"));
}
void GuiTest::comboPopupWheel() {
    Window window(true); window.show(); const auto id = window.editGraph_->addNode("agent");
    window.editView_->setupScale(1.0); window.editView_->centerScene(); QCoreApplication::processEvents();
    auto *combo = window.editGraph_->form(id)->findChild<QComboBox *>("nodeModel");
    QVERIFY(combo && !combo->isEditable());
    for (int i = 0; i < 80; ++i) combo->addItem(QString("Model %1").arg(i), QString("model-%1").arg(i));
    combo->setMaxVisibleItems(6); combo->view()->setMaximumHeight(160);
    combo->showPopup(); QCoreApplication::processEvents();
    auto *list = combo->view(); auto *proxy = list->graphicsProxyWidget();
    if (!proxy) proxy = list->window()->graphicsProxyWidget();
    QVERIFY(proxy); QVERIFY(list->verticalScrollBar()->maximum() > 0);
    const auto point = window.editView_->mapFromScene(proxy->mapToScene(list->viewport()->mapTo(proxy->widget(), list->viewport()->rect().center())));
    const auto scale = window.editView_->transform(); const auto before = list->verticalScrollBar()->value();
    QWheelEvent wheel(QPointF(point), QPointF(window.editView_->viewport()->mapToGlobal(point)), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(window.editView_->viewport(), &wheel); QCoreApplication::processEvents();
    QCOMPARE(window.editView_->transform(), scale); QVERIFY(list->verticalScrollBar()->value() > before);
    list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum());
    const auto canvasPosition = QPoint(window.editView_->horizontalScrollBar()->value(), window.editView_->verticalScrollBar()->value());
    QWheelEvent atEnd(QPointF(point), QPointF(window.editView_->viewport()->mapToGlobal(point)), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(window.editView_->viewport(), &atEnd); QCOMPARE(window.editView_->transform(), scale);
    QCOMPARE(QPoint(window.editView_->horizontalScrollBar()->value(), window.editView_->verticalScrollBar()->value()), canvasPosition);
    combo->hidePopup();
    QWheelEvent canvas(QPointF(5, 5), QPointF(window.editView_->viewport()->mapToGlobal(QPoint(5, 5))), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(window.editView_->viewport(), &canvas); QVERIFY(window.editView_->transform() != scale);
}
void GuiTest::nodeExecutionHistory() {
    QTemporaryDir directory; QVERIFY(directory.isValid()); Window window(true); window.show();
    const auto agent = window.editGraph_->addNode("agent"), branch = window.editGraph_->addNode("branch");
    auto node = window.editGraph_->flow.nodes.at(agent); node.model = "fixture"; node.prompt = "COUNT_TO_3";
    node.title = "ループの実行履歴"; node.schema = R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"]})";
    window.editGraph_->updateNode(node);
    auto condition = window.editGraph_->flow.nodes.at(branch); condition.rules = {{"count", "3", false}}; window.editGraph_->updateNode(condition);
    window.editGraph_->addConnection({1, 0, agent, 0}); window.editGraph_->addConnection({agent, 0, branch, 0});
    window.editGraph_->addConnection({branch, 0, agent, 0}); window.editGraph_->addConnection({branch, 1, 2, 0});
    window.editGraph_->setNodeData(1, NodeRole::Position, QPointF(0, 160)); window.editGraph_->setNodeData(agent, NodeRole::Position, QPointF(180, 40));
    window.editGraph_->setNodeData(branch, NodeRole::Position, QPointF(650, 40)); window.editGraph_->setNodeData(2, NodeRole::Position, QPointF(1100, 160));
    window.projects_.push_back({"履歴のテスト", directory.path(), window.documents_[0].path, {}}); window.refreshProjects();
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent.exe";
#ifndef Q_OS_WIN
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#endif
    window.tabs_->setCurrentIndex(1); window.selectWorkNode(agent);
    auto *split = window.findChild<QSplitter *>("workDetailsSplitter"); QVERIFY(split); QCOMPARE(split->count(), 3); QCOMPARE(split->orientation(), Qt::Vertical);
    QCOMPARE(split->widget(1)->layout()->contentsMargins().top(), 8);
    QCOMPARE(split->widget(2)->layout()->contentsMargins().top(), 8);
    QSignalSpy done(window.runner_, &Runner::finished); window.executeFlow(); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QCOMPARE(window.projects_[0].history.size(), size_t(8)); QCOMPARE(window.historyList_->count(), 3);
    QVERIFY(window.output_->toPlainText().contains('3'));
    window.historyList_->setCurrentRow(2); const auto firstId = window.selectedHistory_;
    QVERIFY(!window.log_->toPlainText().contains("--- 実行"));
    QVERIFY(window.historyList_->item(2)->text().contains("実行 1 · Codex"));
    QVERIFY(!window.historyList_->item(2)->text().contains(node.title));
    QVERIFY(window.output_->toPlainText().contains('1')); QVERIFY(window.historyList_->item(2)->text().contains(QDate::currentDate().toString("yyyy/MM/dd")));
    const auto firstLog = window.log_->toPlainText();
    QVERIFY(QFile::remove(directory.filePath("counter.txt"))); done.clear(); window.executeFlow(); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QCOMPARE(window.projects_[0].history.size(), size_t(16)); QCOMPARE(window.historyList_->count(), 6);
    QCOMPARE(window.selectedHistory_, firstId); QCOMPARE(window.log_->toPlainText(), firstLog); QVERIFY(window.output_->toPlainText().contains('1'));
    window.historyList_->setCurrentRow(0); QVERIFY(window.output_->toPlainText().contains('3'));
    window.selectWorkNode(0); QCOMPARE(window.historyList_->count(), 0);
    QVERIFY(window.log_->toPlainText().isEmpty()); QVERIFY(window.output_->toPlainText().isEmpty());
    QVERIFY(QFile::remove(directory.filePath("counter.txt"))); done.clear(); window.executeFlow(); QTRY_COMPARE(done.count(), 1);
    QCOMPARE(window.workSelected_, Id(0)); QCOMPARE(window.historyList_->count(), 0);
    window.projects_.push_back({"別のプロジェクト", directory.path(), window.documents_[0].path, {}}); window.selectProject(1);
    QCOMPARE(window.historyList_->count(), 0); QVERIFY(window.log_->toPlainText().isEmpty());
    window.selectProject(0); window.selectWorkNode(agent); QCOMPARE(window.historyList_->count(), 9);
    window.historyList_->setCurrentRow(8); QCOMPARE(window.log_->toPlainText(), firstLog);
    QVERIFY(!window.followLatestHistory_); QTest::mouseClick(window.followHistoryButton_, Qt::LeftButton);
    QVERIFY(window.followLatestHistory_); QCOMPARE(window.historyList_->currentRow(), 0);
    window.workView_->setupScale(0.7); window.workView_->centerScene(); QCoreApplication::processEvents();
    QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-node-history.png")));
}
void GuiTest::historyAfterRestart() {
    QTemporaryDir directory; QVERIFY(directory.isValid()); QVERIFY(QDir().mkpath(directory.filePath("flows")));
    Window original(true); original.dataDir_ = directory.path();
    original.documents_[0].path = directory.filePath("flows/test.json"); original.documents_[0].flow.save(original.documents_[0].path);
    original.projects_.push_back({"保存テスト", directory.path(), original.documents_[0].path, {}}); original.refreshProjects();
    NodeExecution entry; entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces); entry.node = 1; entry.title = "保存したノード";
    entry.startedAt = QDateTime::currentDateTimeUtc(); entry.finishedAt = entry.startedAt.addMSecs(10);
    entry.state = State::Succeeded; entry.log = "保存したログ\n日本語"; entry.output = {{"saved", true}};
    original.smoke_ = false; original.recordNodeExecution(entry); original.persist(); original.smoke_ = true;
    QVERIFY(original.pendingHistory_.empty());
    Window restored(true); restored.dataDir_ = directory.path(); restored.documents_.clear(); restored.projects_.clear(); restored.restore();
    QCOMPARE(restored.projects_.size(), size_t(1)); QCOMPARE(restored.projects_[0].historyId, original.projects_[0].historyId);
    QCOMPARE(restored.projects_[0].history.size(), size_t(1)); QCOMPARE(restored.projects_[0].history[0].startedAt, entry.startedAt);
    restored.selectFlow(0); restored.selectWorkNode(1); QCOMPARE(restored.historyList_->count(), 1);
    QCOMPARE(restored.log_->toPlainText(), entry.log); QVERIFY(restored.output_->toPlainText().contains("true"));
    QCOMPARE(restored.selectedHistory_, entry.id);
    Graph graph; graph.replace(Flow::initial()); const auto deleted = graph.addNode("agent"); QVERIFY(graph.deleteNode(deleted));
    graph.replace(Flow::fromJson(graph.flow.toJson())); QVERIFY(graph.addNode("agent") > deleted);
}
void GuiTest::scriptNodeForm() {
    Window window(true); window.resize(1400, 950); window.show();
    auto *add = window.findChild<QPushButton *>("addNode_script"); QVERIFY(add); QTest::mouseClick(add, Qt::LeftButton);
    const auto id = window.selected_; QVERIFY(window.editGraph_->nodeExists(id)); QVERIFY(window.editGraph_->flow.nodes.at(id).kind == Kind::Script);
    auto *form = window.editGraph_->form(id); QVERIFY(form);
    auto *language = form->findChild<QComboBox *>("scriptLanguage"); auto *path = form->findChild<QLineEdit *>("scriptPath"); auto *directory = form->findChild<QLineEdit *>("scriptDirectory");
    QVERIFY(language && path && directory); QVERIFY(!form->findChild<QComboBox *>("nodeModel")); QCOMPARE(language->currentData().toString(), QString("python"));
    language->setCurrentIndex(language->findData("javascript"));
    path->setFocus(); QTest::keyClicks(path, "scripts/transform.js"); directory->setFocus(); QTest::keyClicks(directory, "work dir");
    const auto node = window.editGraph_->flow.nodes.at(id);
    QCOMPARE(node.scriptLanguage, QString("javascript")); QCOMPARE(node.scriptPath, QString("scripts/transform.js")); QCOMPARE(node.scriptDirectory, QString("work dir"));
    auto updated = node; updated.scriptLanguage = "python"; updated.scriptPath = "scripts/transform.py"; updated.scriptDirectory.clear(); window.editGraph_->updateNode(updated);
    QCOMPARE(language->currentData().toString(), QString("python")); QCOMPARE(path->text(), updated.scriptPath); QVERIFY(directory->text().isEmpty());
    Graph restored; restored.loadNode(window.editGraph_->saveNode(id)); QCOMPARE(restored.flow.nodes.at(id).scriptPath, updated.scriptPath);
    Graph work; work.readOnly = true; work.replace(window.editGraph_->flow); auto *readOnly = work.form(id); readOnly->show();
    auto *workPath = readOnly->findChild<QLineEdit *>("scriptPath"); auto *workLanguage = readOnly->findChild<QComboBox *>("scriptLanguage");
    QVERIFY(workPath->isReadOnly()); const auto before = workLanguage->currentIndex();
    QTest::keyClicks(workPath, "changed"); QTest::keyClick(workLanguage, Qt::Key_Down); QCOMPARE(workPath->text(), updated.scriptPath); QCOMPARE(workLanguage->currentIndex(), before);
    window.projects_.push_back({"Script project", QDir::currentPath(), window.documents_[0].path, {}}); window.refreshProjects(); window.selectProject(0); window.selectWorkNode(id);
    NodeExecution execution; execution.id = QUuid::createUuid().toString(QUuid::WithoutBraces); execution.node = id; execution.agent = "Python";
    execution.log = "{\"type\":\"system\",\"subtype\":\"init\"}\n"; execution.startedAt = QDateTime::currentDateTimeUtc(); execution.state = State::Succeeded; execution.output = "Script output";
    window.recordNodeExecution(execution);
    auto *logHeading = window.log_->findChild<QLabel *>("progressHeading"); QVERIFY(logHeading && logHeading->text().contains("system"));
    window.tabs_->setCurrentIndex(0);
    window.editView_->centerScene(); QCoreApplication::processEvents(); QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-script-node.png")));
}
void GuiTest::scriptPathDialogs() {
    const bool hadOption = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    auto restore = qScopeGuard([&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, hadOption); });
    QTemporaryDir directory; QVERIFY(directory.isValid()); QVERIFY(QDir(directory.path()).mkpath("日本語 work dir"));
    const QString pythonPath = directory.filePath("日本語 script.py"), jsPath = directory.filePath("script.mjs");
    for (const auto &path : {pythonPath, jsPath}) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("# selected only; not executed\n"); }
    Graph graph; graph.replace(Flow::initial()); const auto id = graph.addNode("script"); auto *form = graph.form(id); QVERIFY(form); form->show();
    auto *fileButton = form->findChild<QPushButton *>("scriptFileBrowse"), *directoryButton = form->findChild<QPushButton *>("scriptDirectoryBrowse");
    QVERIFY(fileButton && directoryButton); QSignalSpy edits(&graph, &Graph::edited);
    auto select = [&](QPushButton *button, const QString &objectName, const QString &path, bool accept) {
        bool handled = false;
        QTimer::singleShot(0, form, [&] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog); QCOMPARE(dialog->objectName(), objectName);
            handled = true;
            if (!accept) { dialog->reject(); return; }
            if (objectName == "scriptFileDialog") {
                QVERIFY(dialog->nameFilters().first().contains(graph.flow.nodes.at(id).scriptLanguage == "python" ? "*.py" : "*.mjs"));
                dialog->selectFile(path);
            } else { QCOMPARE(dialog->fileMode(), QFileDialog::Directory); dialog->setDirectory(path); }
            QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
        });
        QTest::mouseClick(button, Qt::LeftButton); QVERIFY(handled);
    };
    select(fileButton, "scriptFileDialog", pythonPath, true);
    QCOMPARE(QDir::cleanPath(graph.flow.nodes.at(id).scriptPath), QDir::cleanPath(pythonPath)); QCOMPARE(edits.count(), 1);
    select(directoryButton, "scriptDirectoryDialog", directory.filePath("日本語 work dir"), true);
    QCOMPARE(QDir::cleanPath(graph.flow.nodes.at(id).scriptDirectory), QDir::cleanPath(directory.filePath("日本語 work dir"))); QCOMPARE(edits.count(), 2);
    select(fileButton, "scriptFileDialog", {}, false); select(directoryButton, "scriptDirectoryDialog", {}, false); QCOMPARE(edits.count(), 2);
    auto *language = form->findChild<QComboBox *>("scriptLanguage"); language->setCurrentIndex(language->findData("javascript"));
    select(fileButton, "scriptFileDialog", jsPath, true); QCOMPARE(QDir::cleanPath(graph.flow.nodes.at(id).scriptPath), QDir::cleanPath(jsPath));
    form->setReadOnly(true); QVERIFY(!fileButton->isEnabled() && !directoryButton->isEnabled());
    form->setReadOnly(false); QVERIFY(fileButton->isEnabled() && directoryButton->isEnabled());
}
void GuiTest::shellNodeForm() {
    const bool native = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs); QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    auto restore = qScopeGuard([&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, native); });
    QTemporaryDir directory; for (const auto &name : {"flows", "project", "chosen"}) QVERIFY(QDir(directory.path()).mkpath(name));
    Window window(true); window.resize(1400, 950); window.show();
    auto *add = window.findChild<QPushButton *>("addNode_shell"); QVERIFY(add); QTest::mouseClick(add, Qt::LeftButton);
    const auto id = window.selected_; QVERIFY(window.editGraph_->flow.nodes.at(id).kind == Kind::Shell);
    auto *form = window.editGraph_->form(id); QVERIFY(form); auto *preview = form->findChild<TextPreview *>("shellCommandPreview"); QVERIFY(preview);
    QTest::mouseClick(preview->viewport(), Qt::LeftButton); QCOMPARE(window.field_, QString("shellCommand")); QCOMPARE(window.textTitle_->text(), QString("シェルコマンド"));
    window.text_->setPlainText("echo hello\necho world"); QCOMPARE(window.editGraph_->flow.nodes.at(id).shellCommand, QString("echo hello\necho world"));
    QCOMPARE(preview->toPlainText(), QString("echo hello\necho world")); QVERIFY(!form->findChild<QComboBox *>("nodeModel"));
    auto *browse = form->findChild<QPushButton *>("shellDirectoryBrowse"); auto *path = form->findChild<QLineEdit *>("shellDirectory"); QVERIFY(browse && path);
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog); QCOMPARE(dialog->objectName(), QString("shellDirectoryDialog"));
        dialog->setDirectory(directory.filePath("chosen")); QVERIFY(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection));
    });
    QTest::mouseClick(browse, Qt::LeftButton); QCOMPARE(QDir::cleanPath(path->text()), QDir::cleanPath(directory.filePath("chosen")));
    QTimer::singleShot(0, &window, [] { if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) dialog->reject(); });
    QTest::mouseClick(browse, Qt::LeftButton); QCOMPARE(QDir::cleanPath(path->text()), QDir::cleanPath(directory.filePath("chosen")));
    path->setFocus(); path->selectAll(); QTest::keyClick(path, Qt::Key_Backspace); QVERIFY(window.editGraph_->flow.nodes.at(id).shellDirectory.isEmpty());
    window.selectNode(id); window.showText("shellCommand");
#ifdef Q_OS_WIN
    window.text_->setPlainText("[Console]::Write((Get-Location).Path)");
#else
    window.text_->setPlainText("pwd");
#endif
    window.editGraph_->addConnection({1, 0, id, 0}); window.editGraph_->addConnection({id, 0, 2, 0});
    const auto flowPath = directory.filePath("flows/flow.json"); window.documents_[0].path = flowPath; window.editGraph_->flow.save(flowPath);
    window.projects_.push_back({"Shell project", directory.filePath("project"), flowPath, {}}); window.refreshProjects(); window.selectProject(0);
    window.tabs_->setCurrentIndex(1); window.selectWorkNode(id);
    auto *workForm = window.workGraph_->form(id); QVERIFY(workForm->findChild<QLineEdit *>("shellDirectory")->isReadOnly()); QVERIFY(!workForm->findChild<QPushButton *>("shellDirectoryBrowse")->isEnabled());
    QSignalSpy done(window.runner_, &Runner::finished); window.executeFlow(); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    const auto stdoutText = QString::fromStdString(window.runner_->results().at(2).output.at("stdout")).trimmed();
    QCOMPARE(QDir::fromNativeSeparators(stdoutText), QFileInfo(directory.filePath("flows")).canonicalFilePath());
    window.tabs_->setCurrentIndex(0); window.editView_->centerScene(); QCoreApplication::processEvents();
    QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-shell-node.png")));
}
}
QTEST_MAIN(an::GuiTest)
