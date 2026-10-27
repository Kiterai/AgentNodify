#include "gui_test.h"
#include "window.h"
#include "nodeform.h"
#include "graphview.h"
#include <QSystemTrayIcon>
#include "theme.h"
#include "projectopener.h"
#include "history.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
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
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>

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
void GuiTest::removedOutputsPreservePaths_data() {
    QTest::addColumn<bool>("branch"); QTest::addColumn<bool>("duplicates"); QTest::addColumn<int>("removed");
    for(bool branch:{false,true}) for(bool duplicates:{false,true}) for(int removed:{0,1,2})
        QTest::newRow(qPrintable(QString("%1 %2 remove %3").arg(branch?"branch":"user",duplicates?"duplicates":"unique").arg(removed))) << branch << duplicates << removed;
}
void GuiTest::removedOutputsPreservePaths() {
    QFETCH(bool,branch); QFETCH(bool,duplicates); QFETCH(int,removed);
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial()); const auto id=graph.addNode(branch?"branch":"user_input");
    auto node=graph.flow.nodes.at(id); node.rules.clear(); node.userChoices.clear();
    for(int i=0;i<3;++i) { const auto name=duplicates?QString("Same"):QString::number(i); if(branch) node.rules.push_back({"x",name,true}); else node.userChoices.push_back(name); }
    graph.updateNode(node); graph.addConnection({1,0,id,0}); std::vector<Id> targets;
    for(unsigned port=0;port<graph.flow.outputCount(node);++port) {
        const auto target=graph.addNode("branch"); auto pass=graph.flow.nodes.at(target); pass.rules.clear(); graph.updateNode(pass); targets.push_back(target);
        graph.addConnection({id,port,target,0}); graph.addConnection({id,port,2,0}); graph.addConnection({target,0,2,0});
    }
    graph.addConnection({id,1,id,0}); const auto original=graph.flow.edges; auto *incoming=scene.connectionGraphicsObject({1,0,id,0}); auto *first=scene.connectionGraphicsObject({id,0,targets[0],0});
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); auto buttons=graph.form(id)->findChildren<QPushButton *>(branch?"removeRule":"removeChoice"); QCOMPARE(buttons.size(),3);
    QTest::mouseClick(buttons[removed],Qt::LeftButton);
    std::vector<Edge> expected;
    for(auto edge:original) { if(edge.from==id && edge.output==unsigned(removed)) continue; if(edge.from==id && edge.output>unsigned(removed)) --edge.output; expected.push_back(edge); }
    QVERIFY(graph.flow.edges==expected); QCOMPARE(scene.connectionGraphicsObject({1,0,id,0}),incoming);
    if(removed!=0) QCOMPARE(scene.connectionGraphicsObject({id,0,targets[0],0}),first);
    QVERIFY(!scene.connectionGraphicsObject({id,unsigned(removed),targets[size_t(removed)],0}));
    for(const auto &edge:expected) QVERIFY(scene.connectionGraphicsObject({edge.from,edge.output,edge.to,edge.input}));
    if(branch) { QCOMPARE(graph.portData(id,PortType::Out,2,PortRole::Caption).toString(),QString("それ以外")); QVERIFY(graph.connectionExists({id,2,targets.back(),0})); }
    QVERIFY(Flow::fromJson(graph.flow.toJson()).edges==expected);
}
void GuiTest::bulkPortReduction() {
    for(bool branch:{false,true}) {
        Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial()); const auto id=graph.addNode(branch?"branch":"user_input");
        auto node=graph.flow.nodes.at(id); node.rules.clear(); node.userChoices={"A","B","C","D"}; for(auto name:node.userChoices) node.rules.push_back({"x",name,true}); graph.updateNode(node);
        graph.addConnection({1,0,id,0}); for(unsigned port=0;port<graph.flow.outputCount(node);++port) graph.addConnection({id,port,2,0});
        node.userChoices={"B","D"}; node.rules={node.rules[1],node.rules[3]}; graph.updateNode(node);
        QCOMPARE(graph.flow.edges.size(),branch?size_t(4):size_t(3)); QVERIFY(graph.connectionExists({1,0,id,0})); QVERIFY(graph.connectionExists({id,0,2,0})); QVERIFY(graph.connectionExists({id,1,2,0}));
        if(branch) { QVERIFY(graph.connectionExists({id,2,2,0})); node.rules.clear(); graph.updateNode(node); QCOMPARE(graph.flow.edges.size(),size_t(2)); QVERIFY(graph.connectionExists({id,0,2,0})); }
    }
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial()); const auto id=graph.addNode("all"); auto node=graph.flow.nodes.at(id); node.inputs=3; graph.updateNode(node);
    for(unsigned port=0;port<3;++port) graph.addConnection({1,0,id,port}); graph.addConnection({id,0,2,0}); node.inputs=2; graph.updateNode(node);
    QCOMPARE(graph.flow.edges.size(),size_t(3)); QVERIFY(graph.connectionExists({1,0,id,0})); QVERIFY(graph.connectionExists({1,0,id,1})); QVERIFY(graph.connectionExists({id,0,2,0}));
}
void GuiTest::reducedUserInputExecutesSurvivingPath() {
    QTemporaryDir directory; Graph graph; BasicGraphicsScene scene(graph); auto flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; input.userChoices={"Same","Same","Same"}; flow.nodes[3]=input;
    for(Id id:{4u,5u,6u}) { Node pass; pass.id=id; pass.kind=Kind::Branch; flow.nodes[id]=pass; }
    flow.edges={{1,3},{3,4,0,0},{3,5,1,0},{3,6,2,0},{4,2},{5,2},{6,2}}; graph.replace(flow);
    auto removes=graph.form(3)->findChildren<QPushButton *>("removeChoice"); QTest::mouseClick(removes[1],Qt::LeftButton);
    graph.deleteNode(5); QVERIFY(graph.flow.validate().isEmpty()); Runner runner; QSignalSpy done(&runner,&Runner::finished);
    connect(&runner,&Runner::userInputRequested,&runner,[&](const UserInputRequest &request){QVERIFY(runner.submitUserInput(request.id,"answer",1));});
    QVERIFY(runner.start(graph.flow,directory.path(),{})); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QCOMPARE(runner.results().at(6).executions,quint64(1)); QVERIFY(runner.results().at(4).state==State::Skipped); QVERIFY(runner.results().at(2).output=="answer");
}
void GuiTest::appendedOutputsPreservePaths() {
    Graph graph; BasicGraphicsScene scene(graph); graph.replace(Flow::initial()); const auto branch=graph.addNode("branch");
    const auto condition=graph.addNode("branch"), fallback=graph.addNode("branch"), other=graph.addNode("branch");
    graph.addConnection({1,0,branch,0}); graph.addConnection({branch,0,condition,0});
    for(auto target:{fallback,other,branch}) graph.addConnection({branch,1,target,0});
    for(auto target:{condition,fallback,other}) graph.addConnection({target,0,2,0});
    const auto original=graph.flow.edges; auto *incoming=scene.connectionGraphicsObject({1,0,branch,0}), *matched=scene.connectionGraphicsObject({branch,0,condition,0}); QVERIFY(incoming); QVERIFY(matched);
    auto expected=original; for(auto &edge:expected) if(edge.from==branch && edge.output==1) edge.output=2;
    auto *form=graph.form(branch); QTest::mouseClick(form->findChild<QPushButton *>("addRule"),Qt::LeftButton);
    QVERIFY(graph.flow.edges==expected); QCOMPARE(scene.connectionGraphicsObject({1,0,branch,0}),incoming); QCOMPARE(scene.connectionGraphicsObject({branch,0,condition,0}),matched);
    for(auto target:{fallback,other,branch}) { QVERIFY(!scene.connectionGraphicsObject({branch,1,target,0})); QVERIFY(scene.connectionGraphicsObject({branch,2,target,0})); }
    QCOMPARE(graph.portData(branch,PortType::Out,2,PortRole::Caption).toString(),QString("それ以外"));
    auto node=graph.flow.nodes.at(branch); node.rules.push_back({"x","a",true}); node.rules.push_back({"x","b",true}); graph.updateNode(node);
    expected=original; for(auto &edge:expected) if(edge.from==branch && edge.output==1) edge.output=4; QVERIFY(graph.flow.edges==expected);
    for(auto target:{fallback,other,branch}) QVERIFY(scene.connectionGraphicsObject({branch,4,target,0}));
    const auto empty=graph.addNode("branch"); auto noRules=graph.flow.nodes.at(empty); noRules.rules.clear(); graph.updateNode(noRules); graph.addConnection({1,0,empty,0}); graph.addConnection({empty,0,2,0});
    noRules.rules.push_back({"x","a",true}); graph.updateNode(noRules); QVERIFY(graph.connectionExists({1,0,empty,0})); QVERIFY(graph.connectionExists({empty,1,2,0})); QVERIFY(!graph.connectionExists({empty,0,2,0}));
    Graph user; BasicGraphicsScene userScene(user); user.replace(Flow::initial()); const auto input=user.addNode("user_input");
    user.addConnection({1,0,input,0}); user.addConnection({input,0,2,0}); user.addConnection({input,0,input,0}); const auto paths=user.flow.edges;
    auto *inputEdge=userScene.connectionGraphicsObject({1,0,input,0}), *outputEdge=userScene.connectionGraphicsObject({input,0,2,0}), *loopEdge=userScene.connectionGraphicsObject({input,0,input,0});
    QTest::mouseClick(user.form(input)->findChild<QPushButton *>("addChoice"),Qt::LeftButton); QVERIFY(user.flow.edges==paths);
    QCOMPARE(userScene.connectionGraphicsObject({1,0,input,0}),inputEdge); QCOMPARE(userScene.connectionGraphicsObject({input,0,2,0}),outputEdge); QCOMPARE(userScene.connectionGraphicsObject({input,0,input,0}),loopEdge);
    auto choices=user.flow.nodes.at(input); choices.userChoices.append({"A","B","C"}); user.updateNode(choices); QVERIFY(user.flow.edges==paths);
    user.addConnection({input,4,2,0}); const auto extended=user.flow.edges; choices.userChoices.push_back("D"); user.updateNode(choices); QVERIFY(user.flow.edges==extended); QVERIFY(userScene.connectionGraphicsObject({input,4,2,0}));
    QVERIFY(Flow::fromJson(user.flow.toJson()).edges==extended); user.readOnly=true; choices.userChoices.push_back("Ignored"); const auto before=user.flow.toJson(); user.updateNode(choices); QVERIFY(user.flow.toJson()==before);
}
void GuiTest::appendedBranchExecutesElse() {
    QTemporaryDir directory; Graph graph; BasicGraphicsScene scene(graph); auto flow=Flow::initial();
    Node source; source.id=3; source.kind=Kind::Agent; source.model="fixture"; source.prompt="Say hello"; source.schema=R"({"type":"object","properties":{"result":{"type":"string"}},"required":["result"]})"; flow.nodes[3]=source;
    Node branch; branch.id=4; branch.kind=Kind::Branch; branch.rules={{"result","never",true}}; flow.nodes[4]=branch;
    for(Id id:{5u,6u,7u}) { Node pass; pass.id=id; pass.kind=Kind::Branch; flow.nodes[id]=pass; }
    flow.edges={{1,3},{3,4},{4,5,0,0},{4,6,1,0},{5,2},{6,2},{7,2}}; graph.replace(flow);
    branch.rules.push_back({"result","also never",true}); graph.updateNode(branch); graph.addConnection({4,1,7,0}); QVERIFY(graph.flow.validate().isEmpty());
    AgentConfig config; config.codex=QCoreApplication::applicationDirPath()+"/fake_agent";
#ifdef Q_OS_WIN
    config.codex+=".exe";
#endif
    Runner runner; QSignalSpy done(&runner,&Runner::finished); QVERIFY(runner.start(graph.flow,directory.path(),config)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool());
    QCOMPARE(runner.results().at(6).executions,quint64(1)); QVERIFY(runner.results().at(5).state==State::Skipped); QVERIFY(runner.results().at(7).state==State::Skipped);
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
    QCOMPARE(graph.allConnectionIds(join).size(),size_t(1)); QCOMPARE(graph.form(join), joinForm);
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
    QVERIFY(!palette->findChild<QPushButton *>("addNode_start")); QVERIFY(!palette->findChild<QPushButton *>("addNode_end"));
    QCOMPARE(window.editGraph_->flow.nodes.size(), size_t(2)); QVERIFY(window.editGraph_->flow.nodes.at(1).kind == Kind::Start); QVERIFY(window.editGraph_->flow.nodes.at(2).kind == Kind::End);
    for (auto kind : {Kind::Agent, Kind::Script, Kind::Shell, Kind::All, Kind::Any, Kind::Branch}) {
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
    node.title = "ループの実行履歴"; node.retainContext = true; node.schema = R"({"type":"object","properties":{"count":{"type":"integer"}},"required":["count"]})";
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
void GuiTest::simplifiedCliSettings() {
    Window window(true); window.show();
    bool inspected = false;
    QTimer::singleShot(0, &window, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog); QCOMPARE(dialog->windowTitle(), QString("CLI設定"));
        QVERIFY(dialog->findChildren<QSpinBox *>().isEmpty()); QVERIFY(dialog->findChildren<QCheckBox *>().isEmpty());
        QVERIFY(dialog->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-cli-settings.png")));
        const auto edits = dialog->findChildren<QLineEdit *>(); QCOMPARE(edits.size(), 3);
        edits[0]->setText("custom-codex"); edits[1]->setText("custom-claude"); edits[2]->setText("custom-opencode");
        inspected = true; dialog->accept();
    });
    window.settings(); QVERIFY(inspected); QCOMPARE(window.config_.codex, QString("custom-codex")); QCOMPARE(window.config_.claude, QString("custom-claude")); QCOMPARE(window.config_.opencode, QString("custom-opencode"));
    QTemporaryDir directory; const auto workspace = directory.filePath("workspace.json");
    QFile file(workspace); QVERIFY(file.open(QIODevice::WriteOnly));
    const Json legacy = {{"flows", Json::array()}, {"projects", Json::array()}, {"codex", "custom-codex"}, {"claude", "custom-claude"}, {"opencode", "custom-opencode"}, {"timeout", 1}, {"workspaceWrite", false}};
    file.write(QByteArray::fromStdString(legacy.dump())); file.close();
    Window restored(true); restored.dataDir_ = directory.path(); restored.documents_.clear(); restored.projects_.clear(); restored.restore();
    QCOMPARE(restored.config_.codex, QString("custom-codex"));
    restored.smoke_ = false; restored.persist(); restored.smoke_ = true;
    QVERIFY(file.open(QIODevice::ReadOnly)); const auto current = Json::parse(file.readAll().toStdString());
    QVERIFY(!current.contains("timeout")); QVERIFY(!current.contains("workspaceWrite"));
    QCOMPARE(QString::fromStdString(current.at("claude")), QString("custom-claude"));
}
void GuiTest::projectStatusIndicators() {
    QTemporaryDir directory; Window window(true); window.show();
    const auto flowPath = window.documents_[0].path;
    for (QString name : {"Running project", "Other project", "Idle project"}) {
        const auto path = directory.filePath(name); QVERIFY(QDir().mkpath(path)); window.projects_.push_back({name,path,flowPath,{}});
    }
    auto icon = [&](int index, const char *name) -> QLabel * {
        for (int row = 0; row < window.projectList_->count(); ++row) if (window.projectList_->item(row)->data(Qt::UserRole).toInt() == index)
            return window.projectList_->itemWidget(window.projectList_->item(row))->findChild<QLabel *>(name);
        return nullptr;
    };
    auto verify = [&](int index, bool running, bool waiting) {
        auto *run = icon(index,"projectRunningIcon"), *input = icon(index,"projectInputWaitingIcon"); QVERIFY(run); QVERIFY(input);
        QCOMPARE(run->property("active").toBool(),running); QCOMPARE(input->property("active").toBool(),waiting);
        QCOMPARE(!run->pixmap().isNull(),running); QCOMPARE(!input->pixmap().isNull(),waiting);
        if (running) { QVERIFY(run->toolTip().contains("実行中")); QVERIFY(run->accessibleName().contains("実行中")); }
        if (waiting) { QVERIFY(input->toolTip().contains("入力待ち")); QVERIFY(input->accessibleName().contains("入力待ち")); }
        auto colored = [](const QImage &image, bool amber) {
            int pixels=0;
            for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
                const auto color=image.pixelColor(x,y);
                if(amber ? (color.red()>180 && color.green()>140 && color.blue()<160) : (color.blue()>180 && color.blue()>color.red()+30)) ++pixels;
            }
            return pixels>5;
        };
        QCOMPARE(colored(run->grab().toImage(),false),running);
        QCOMPARE(colored(input->grab().toImage(),true),waiting);
    };
    auto flow = Flow::initial(); Node agent; agent.id=3; agent.kind=Kind::Agent; agent.model="fixture"; agent.prompt="SLEEP"; flow.nodes[3]=agent; flow.edges={{1,3},{3,2}};
    window.editGraph_->replace(flow); window.changed(); window.refreshProjects(); window.tabs_->setCurrentIndex(1);
    for (int index=0; index<3; ++index) verify(index,false,false);
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    window.executeFlow(); QTRY_VERIFY(window.runner_->results().at(3).state==State::Running); verify(0,true,false); verify(1,false,false);
    QVERIFY(window.coordinator_->running(window.projects_[0].historyId)); QVERIFY(!window.coordinator_->running(window.projects_[1].historyId));
    window.coordinator_->cancelAll(); QTRY_VERIFY(!window.busy()); verify(0,false,false);
    flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; flow.nodes[3]=input; flow.edges={{1,3},{3,2}};
    window.editGraph_->replace(flow); window.changed(); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); verify(0,true,true); verify(1,false,false);
    QVERIFY(window.coordinator_->start(window.coordinator_->projects[1])); QTRY_COMPARE(window.inputPrompts_.size(),size_t(2)); verify(0,true,true); verify(1,true,true); verify(2,false,false);
    QVERIFY(!window.projectList_->isEnabled()); QVERIFY(window.projectList_->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-project-status-icons.png")));
    QString first,second;
    for (const auto &prompt:window.inputPrompts_) if(prompt.projectId==window.projects_[0].historyId) first=prompt.request.id; else second=prompt.request.id;
    QVERIFY(window.coordinator_->submitUserInput(first,"first",0)); QTRY_VERIFY(!window.coordinator_->running(window.projects_[0].historyId)); QVERIFY(window.busy()); verify(0,false,false); verify(1,true,true);
    QVERIFY(window.coordinator_->submitUserInput(second,"second",0)); QTRY_VERIFY(!window.busy()); verify(0,false,false); verify(1,false,false);
    window.projects_[1].results[3].state=State::Running; window.refreshProjectStatusIcons(); verify(1,false,false); // Historical results are not live status.
    window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); verify(0,true,true); window.coordinator_->cancelAll(); QTRY_VERIFY(!window.busy()); verify(0,false,false);
    flow=Flow::initial(); flow.nodes[3]=input; input.id=4; flow.nodes[4]=input;
    Node join; join.id=5; join.kind=Kind::All; flow.nodes[5]=join; flow.edges={{1,3},{1,4},{3,5,0,0},{4,5,0,1},{5,2}};
    window.editGraph_->replace(flow); window.changed(); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(2)); verify(0,true,true); QVERIFY(icon(0,"projectInputWaitingIcon")->toolTip().contains("2件"));
    const auto one=window.inputPrompts_[0].request.id, two=window.inputPrompts_[1].request.id;
    QVERIFY(window.coordinator_->submitUserInput(one,"one",0)); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); verify(0,true,true); QVERIFY(icon(0,"projectInputWaitingIcon")->toolTip().contains("1件"));
    QVERIFY(window.coordinator_->submitUserInput(two,"two",0)); QTRY_VERIFY(!window.busy()); verify(0,false,false);
    flow=Flow::initial(); input.id=3; flow.nodes[3]=input; flow.edges={{1,3},{3,2}};
    Node wait; wait.id=4; wait.kind=Kind::FlowWait; wait.targetFlow=flowPath; wait.projectPath="."; flow.nodes[4]=wait; flow.edges.push_back({1,4}); flow.edges.push_back({4,2});
    window.editGraph_->replace(flow); window.changed(); QSignalSpy failed(window.runner_,&Runner::finished); window.executeFlow(); QTRY_COMPARE(failed.count(),1); QVERIFY(!failed[0][0].toBool()); verify(0,false,false); QVERIFY(window.inputPrompts_.empty());
}
void GuiTest::createdProjectStatusIcons() {
    QTemporaryDir directory; Window window(true); window.show(); const auto path=window.documents_[0].path;
    auto flow=Flow::initial(); Node create; create.id=3; create.kind=Kind::ProjectCreate; create.targetFlow=path; create.projectPath="generated"; flow.nodes[3]=create;
    Node input; input.id=4; input.kind=Kind::UserInput; flow.nodes[4]=input; flow.edges={{1,3},{3,4},{4,2}};
    window.editGraph_->replace(flow); window.changed(); window.projects_.push_back({"Parent",directory.path(),path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.selectWorkNode(4);
    window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); QCOMPARE(window.projectList_->count(),2); QCOMPARE(window.project_,0); QCOMPARE(window.workSelected_,4u);
    auto *parent=window.projectList_->itemWidget(window.projectList_->item(0)), *created=window.projectList_->itemWidget(window.projectList_->item(1));
    QVERIFY(parent->findChild<QLabel *>("projectRunningIcon")->property("active").toBool()); QVERIFY(parent->findChild<QLabel *>("projectInputWaitingIcon")->property("active").toBool());
    QVERIFY(!created->findChild<QLabel *>("projectRunningIcon")->property("active").toBool()); QVERIFY(!created->findChild<QLabel *>("projectInputWaitingIcon")->property("active").toBool());
    window.coordinator_->cancelAll(); QVERIFY(!window.busy());
}
void GuiTest::userInputNodeForm() {
    Window window(true); window.show(); auto *button = window.findChild<QPushButton *>("addNode_user_input"); QVERIFY(button); QTest::mouseClick(button,Qt::LeftButton);
    const auto id = window.selected_; auto *form = window.editGraph_->form(id); QVERIFY(window.editGraph_->flow.nodes.at(id).kind == Kind::UserInput);
    auto names = form->findChildren<QLineEdit *>("choiceName"); QCOMPARE(names.size(),1); QVERIFY(!form->findChild<QPushButton *>("removeChoice")->isEnabled());
    window.editGraph_->addConnection({1,0,id,0}); window.editGraph_->addConnection({id,0,2,0});
    names[0]->selectAll(); QTest::keyClicks(names[0],"Approve"); QCOMPARE(window.editGraph_->portData(id,PortType::Out,0,PortRole::Caption).toString(),QString("Approve")); QCOMPARE(window.editGraph_->allConnectionIds(id).size(),size_t(2));
    QTest::mouseClick(form->findChild<QPushButton *>("addChoice"),Qt::LeftButton); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    QCOMPARE(window.editGraph_->flow.nodes.at(id).userChoices.size(),2); QCOMPARE(window.editGraph_->allConnectionIds(id).size(),size_t(2));
    auto removes = form->findChildren<QPushButton *>("removeChoice"); QCOMPARE(removes.size(),2); QTest::mouseClick(removes.last(),Qt::LeftButton); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    QCOMPARE(window.editGraph_->flow.nodes.at(id).userChoices.size(),1); QVERIFY(!form->findChild<QPushButton *>("removeChoice")->isEnabled());
    window.workGraph_->replace(window.editGraph_->flow); auto *readonly = window.workGraph_->form(id);
    QVERIFY(readonly->findChild<QLineEdit *>("choiceName")->isReadOnly()); QVERIFY(!readonly->findChild<QPushButton *>("addChoice")->isEnabled());
    for (auto state : {State::Pending,State::Running,State::Succeeded}) {
        window.editGraph_->states[id] = state; NodeStyle style(QJsonObject::fromVariantMap(window.editGraph_->nodeData(id,NodeRole::Style).toMap())); QCOMPARE(style.GradientColor0,QColor("#f2a4a4")); QCOMPARE(style.FontColor,QColor("#40242c"));
    }
    window.editGraph_->setNodeData(1,NodeRole::Position,QPointF(0,140)); window.editGraph_->setNodeData(id,NodeRole::Position,QPointF(250,30)); window.editGraph_->setNodeData(2,NodeRole::Position,QPointF(850,140));
    form->grab();
    for (auto *edit : form->findChildren<QLineEdit *>()) QTRY_VERIFY2(edit->height() >= edit->sizeHint().height(),qPrintable(QString("edit height %1 / hint %2, form %3 / hint %4").arg(edit->height()).arg(edit->sizeHint().height()).arg(form->height()).arg(form->sizeHint().height())));
    static_cast<GraphView *>(window.editView_)->zoomToFit(); QTest::qWait(50); window.editView_->viewport()->repaint(); QVERIFY(window.editView_->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-user-input-node.png")));
}
void GuiTest::userInputWorkPanel() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow = Flow::initial();
    Node source; source.id = 3; source.kind = Kind::Agent; source.model = "fixture"; source.prompt = "Say hello"; flow.nodes[3] = source;
    Node input; input.id = 4; input.kind = Kind::UserInput; input.userChoices = {"送信", "別の経路"}; flow.nodes[4] = input;
    for (Id id : {5u,6u}) { Node pass; pass.id = id; pass.kind = Kind::Branch; flow.nodes[id] = pass; }
    flow.edges = {{1,3},{3,4},{4,5,0,0},{4,6,1,0},{5,2},{6,2}};
    flow.nodes[1].position = {-150,140}; flow.nodes[3].position = {0,0}; flow.nodes[4].position = {450,0}; flow.nodes[5].position = {900,-160}; flow.nodes[6].position = {900,250}; flow.nodes[2].position = {1400,140};
    window.editGraph_->replace(flow); window.changed();
    window.projects_.push_back({"Input project",directory.path(),window.documents_[0].path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.selectWorkNode(4);
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    QSignalSpy done(window.runner_,&Runner::finished); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1));
    QVERIFY(window.inputPanel_->isVisible()); QVERIFY(window.inputMessage_->toPlainText().contains("hello")); QVERIFY(window.inputText_->isEnabled()); QVERIFY(window.busy());
    QVERIFY(window.inputPanel_->geometry().top() > window.workView_->geometry().bottom());
    const QString answer = "日本語の回答\n{\"value\":42}"; window.inputText_->setPlainText(answer);
    QVERIFY(!window.close()); QVERIFY(window.busy()); window.showWindow(); QCOMPARE(window.inputText_->toPlainText(),answer);
    QCoreApplication::processEvents(); static_cast<GraphView *>(window.workView_)->zoomToFit(); QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-user-input-work.png")));
    QPushButton *choice = nullptr;
    for (auto *candidate : window.inputPanel_->findChildren<QPushButton *>("userInputChoice")) if (candidate->property("choiceIndex").toInt()==1 && candidate->isVisible()) choice=candidate;
    QVERIFY(choice); QCOMPARE(choice->text(),QString("別の経路")); QTest::mouseClick(choice,Qt::LeftButton);
    QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(window.inputPrompts_.empty()); QVERIFY(!window.inputPanel_->isVisible());
    QVERIFY(window.runner_->results().at(2).output == answer.toUtf8().toStdString()); QVERIFY(window.runner_->results().at(5).state == State::Skipped); QCOMPARE(window.runner_->results().at(6).executions,quint64(1)); QCOMPARE(window.historyList_->count(),1);
    done.clear(); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); window.coordinator_->cancelAll(); QTRY_COMPARE(done.count(),1); QVERIFY(window.inputPrompts_.empty()); QVERIFY(!window.inputPanel_->isVisible());
}
void GuiTest::userInputParallelProjects() {
    QTemporaryDir directory; Window window(true); window.show(); auto child = Flow::initial();
    Node input; input.id = 3; input.kind = Kind::UserInput; child.nodes[3] = input; child.edges = {{1,3},{3,2}};
    const auto childPath = directory.filePath("child.flow.json"); auto parent = Flow::initial();
    Node run; run.id = 3; run.kind = Kind::FlowRun; run.targetFlow = childPath; run.projectPath = "."; parent.nodes[3] = run;
    input.id = 4; parent.nodes[4] = input; parent.edges = {{1,3},{3,4},{4,2}};
    window.editGraph_->replace(parent); window.changed(); window.documents_.push_back({child,childPath,false});
    window.projects_.push_back({"Parent",directory.path(),window.documents_[0].path,{}}); window.projects_.push_back({"Child",directory.path(),childPath,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1);
    QSignalSpy done(window.runner_,&Runner::finished); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(2));
    const auto firstId = window.inputSelector_->currentData().toString(); window.inputText_->setPlainText("draft one"); window.inputSelector_->setCurrentIndex(1); const auto secondId = window.inputSelector_->currentData().toString();
    QVERIFY(firstId != secondId); window.inputText_->setPlainText("draft two"); window.inputSelector_->setCurrentIndex(0); QCOMPARE(window.inputText_->toPlainText(),QString("draft one")); window.inputSelector_->setCurrentIndex(1); QCOMPARE(window.inputText_->toPlainText(),QString("draft two"));
    QString parentRequest, childRequest;
    for (const auto &prompt : window.inputPrompts_) { if (prompt.projectId == window.projects_[0].historyId) parentRequest=prompt.request.id; else childRequest=prompt.request.id; }
    QVERIFY(!parentRequest.isEmpty()); QVERIFY(!childRequest.isEmpty());
    QVERIFY(window.coordinator_->submitUserInput(parentRequest,"parent answer",0)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(window.busy()); QVERIFY(!window.runButton_->isEnabled());
    QCOMPARE(window.inputPrompts_.size(),size_t(1)); QCOMPARE(window.inputSelector_->currentData().toString(),childRequest); QVERIFY(window.inputSelector_->currentText().contains("Child"));
    QVERIFY(window.coordinator_->submitUserInput(childRequest,"child answer",0)); QTRY_VERIFY(!window.busy()); QVERIFY(window.inputPrompts_.empty()); QVERIFY(!window.inputPanel_->isVisible());
    QVERIFY(window.projects_[0].results.at(2).output == "parent answer"); QVERIFY(window.projects_[1].results.at(2).output == "child answer"); QCOMPARE(window.projects_[1].history.size(),size_t(3));
}
void GuiTest::contextCheckboxColors() {
    Node node; node.id = 3; node.kind = Kind::Agent; NodeForm form(node); form.show(); QCoreApplication::processEvents();
    auto *checkbox = form.findChild<QCheckBox *>("retainContext"); QVERIFY(checkbox);
    auto whitePixels = [](const QImage &image, int left, int right) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) for (int x = left; x < qMin(right, image.width()); ++x) {
            const auto color = image.pixelColor(x, y); if (color.red() > 180 && color.green() > 180 && color.blue() > 180) ++count;
        }
        return count;
    };
    const auto unchecked = checkbox->grab().toImage();
    const auto border = whitePixels(unchecked, 0, 20); QVERIFY(border > 10); QVERIFY(whitePixels(unchecked, 24, unchecked.width()) > 20);
    checkbox->setChecked(true); const auto checked = checkbox->grab().toImage(); QVERIFY(whitePixels(checked, 0, 20) > border + 5);
    form.setReadOnly(true); const auto readonly = checkbox->grab().toImage();
    QVERIFY(!checkbox->isEnabled()); QVERIFY(whitePixels(readonly, 0, 20) > 10); QVERIFY(whitePixels(readonly, 24, readonly.width()) > 20);
    QVERIFY(form.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-context-checkbox.png")));
}
void GuiTest::agentContextSetting() {
    QTemporaryDir directory; Window window(true); window.show();
    const auto id = window.editGraph_->addNode("agent"); auto *form = window.editGraph_->form(id);
    auto *checkbox = form->findChild<QCheckBox *>("retainContext"); QVERIFY(checkbox); QVERIFY(!checkbox->isChecked());
    QTest::mouseClick(checkbox, Qt::LeftButton); QVERIFY(checkbox->isChecked());
    QVERIFY(window.editGraph_->flow.nodes.at(id).retainContext); QVERIFY(window.documents_[0].flow.nodes.at(id).retainContext);
    auto *agent = form->findChild<QComboBox *>("nodeAgent"); agent->setCurrentText("Claude Code"); QVERIFY(checkbox->isChecked());
    agent->setCurrentText("OpenCode"); QVERIFY(checkbox->isChecked());
    const auto path = directory.filePath("context.flow.json"); window.editGraph_->flow.save(path); QVERIFY(Flow::load(path).nodes.at(id).retainContext);
    window.workGraph_->replace(window.editGraph_->flow); auto *readonly = window.workGraph_->form(id)->findChild<QCheckBox *>("retainContext");
    QVERIFY(readonly); QVERIFY(readonly->isChecked()); QVERIFY(!readonly->isEnabled()); QTest::mouseClick(readonly, Qt::LeftButton); QVERIFY(readonly->isChecked());
    auto node = window.editGraph_->flow.nodes.at(id); node.retainContext = false; window.editGraph_->updateNode(node); QVERIFY(!checkbox->isChecked());
    const auto script = window.editGraph_->addNode("script"); QVERIFY(!window.editGraph_->form(script)->findChild<QCheckBox *>("retainContext"));
}
void GuiTest::allAndAnyForms() {
    QTemporaryDir directory; Window window(true); window.show();
    auto *all = window.findChild<QPushButton *>("addNode_all"), *any = window.findChild<QPushButton *>("addNode_any");
    QVERIFY(all); QVERIFY(any); QVERIFY(all->text().contains("All")); QVERIFY(any->text().contains("Any")); QVERIFY(!window.findChild<QPushButton *>("addNode_and"));
    QTest::mouseClick(any, Qt::LeftButton); const auto id = window.selected_; auto *form = window.editGraph_->form(id);
    QVERIFY(window.editGraph_->flow.nodes.at(id).kind == Kind::Any);
    auto *inputs = form->findChild<QSpinBox *>("nodeInputs"); QVERIFY(inputs); QCOMPARE(inputs->minimum(), 1); QCOMPARE(inputs->maximum(), 256);
    window.editGraph_->addConnection({1,0,id,0}); inputs->setValue(3); QCOMPARE(window.editGraph_->allConnectionIds(id).size(),size_t(1));
    QCOMPARE(window.editGraph_->flow.nodes.at(id).inputs, 3u);
    QCOMPARE(window.editGraph_->portData(id, PortType::In, 2, PortRole::Caption).toString(), QString("入力 2"));
    inputs->setValue(2); window.editGraph_->addConnection({1,0,id,1}); window.editGraph_->addConnection({1,0,id,0}); window.editGraph_->addConnection({id,0,2,0});
    window.projects_.push_back({"Any", directory.path(), window.documents_[0].path, {}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.selectWorkNode(id);
    auto *readonly = window.workGraph_->form(id)->findChild<QSpinBox *>("nodeInputs"); QVERIFY(readonly); QTest::keyClick(readonly, Qt::Key_Up); QCOMPARE(readonly->value(), 2);
    QSignalSpy done(window.runner_, &Runner::finished); window.executeFlow(); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QCOMPARE(window.historyList_->count(), 2); QVERIFY(window.output_->toPlainText().contains("event")); QVERIFY(window.output_->toPlainText().contains("input"));
    QVERIFY(window.runner_->results().at(2).output == Json({{"event",1},{"input",Json::object()}})); // Preserved port 0 arrives before the newly added port 1.
}
void GuiTest::trayVisibilityAndIdleExit() {
    Window window(true); window.show(); QCoreApplication::processEvents();
    auto *tray = window.findChild<QSystemTrayIcon *>("systemTrayIcon"); QVERIFY(tray); QVERIFY(!tray->icon().isNull()); QCOMPARE(tray->toolTip(), QString("AgentNodify"));
    auto *show = tray->contextMenu()->findChild<QAction *>("trayShow"); auto *quit = tray->contextMenu()->findChild<QAction *>("trayQuit"); QVERIFY(show); QVERIFY(quit);
    QSignalSpy exiting(&window, &Window::quitRequested);
    QVERIFY(!window.close()); QVERIFY(!window.isVisible()); QCOMPARE(exiting.count(), 0); QVERIFY(!window.quitting_);
    tray->activated(QSystemTrayIcon::Context); QVERIFY(!window.isVisible());
    show->trigger(); QVERIFY(window.isVisible());
    window.showMaximized(); QVERIFY(!window.close()); show->trigger(); QVERIFY(window.isMaximized());
    window.showMinimized(); tray->activated(QSystemTrayIcon::DoubleClick); QVERIFY(window.isVisible()); QVERIFY(!window.isMinimized());
    QVERIFY(!window.close()); tray->activated(QSystemTrayIcon::Trigger); QVERIFY(window.isVisible());
    QVERIFY(!window.close()); quit->trigger(); QCOMPARE(exiting.count(), 1); QVERIFY(!window.isVisible()); QVERIFY(window.quitting_);
}
void GuiTest::trayFlowContinues() {
    QTemporaryDir directory; Window window(true); window.show();
    auto flow = Flow::initial(); Node node; node.id = 3; node.kind = Kind::Agent; node.model = "fixture"; node.prompt = "SLEEP";
    flow.nodes[3] = node; flow.edges = {{1,3},{3,2}}; window.editGraph_->replace(flow); window.changed();
    window.projects_.push_back({"Background", directory.path(), window.documents_[0].path, {}}); window.refreshProjects();
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    QSignalSpy done(window.runner_, &Runner::finished), exiting(&window, &Window::quitRequested);
    window.executeFlow(); QTRY_VERIFY(window.runner_->results().at(3).state == State::Running);
    QVERIFY(!window.close()); QVERIFY(!window.isVisible()); QTest::qWait(100); QVERIFY(window.busy()); QCOMPARE(done.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 10000); QVERIFY(done[0][0].toBool());
    QVERIFY(!window.isVisible()); QCOMPARE(exiting.count(), 0); QVERIFY(window.runner_->results().at(2).output == Json("hello"));
    QCOMPARE(window.projects_[0].history.size(), size_t(3));
}
void GuiTest::trayQuitWithRunningFlows_data() {
    QTest::addColumn<bool>("detached"); QTest::newRow("primary") << false; QTest::newRow("detached child") << true;
}
void GuiTest::trayQuitWithRunningFlows() {
    QFETCH(bool, detached); QTemporaryDir directory; Window window(true); window.show();
    auto target = Flow::initial(); Node agent; agent.id = 3; agent.kind = Kind::Agent; agent.model = "fixture"; agent.prompt = "SLEEP";
    target.nodes[3] = agent; target.edges = {{1,3},{3,2}};
    auto parent = target;
    if (detached) { parent.nodes[3].kind = Kind::FlowRun; parent.nodes[3].targetFlow = "child"; parent.nodes[3].projectPath = "."; }
    window.editGraph_->replace(parent); window.changed();
    window.documents_.push_back({target, "child", false});
    window.projects_.push_back({"Parent", directory.path(), window.documents_[0].path, {}});
    window.projects_.push_back({"Child", directory.path(), "child", {}}); window.refreshProjects();
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    window.executeFlow();
    if (detached) QTRY_VERIFY(!window.runner_->running() && window.coordinator_->running());
    else QTRY_VERIFY(window.runner_->results().at(3).state == State::Running);
    QSignalSpy exiting(&window, &Window::quitRequested); QVERIFY(!window.close()); QVERIFY(!window.isVisible());
    auto confirm = [&](QMessageBox::StandardButton answer) {
        QTimer::singleShot(0, &window, [&, answer] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->icon(), QMessageBox::Warning);
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::No)); QVERIFY(box->text().contains("実行中"));
            window.requestQuit(); // Reentrant requests do not create another dialog.
            box->button(answer)->click();
        });
        window.trayIcon_->contextMenu()->findChild<QAction *>("trayQuit")->trigger();
    };
    confirm(QMessageBox::No); QCOMPARE(exiting.count(), 0); QVERIFY(window.busy()); QVERIFY(!window.quitting_); QVERIFY(window.isVisible());
    QVERIFY(!window.close()); confirm(QMessageBox::Yes); QCOMPARE(exiting.count(), 1); QVERIFY(!window.busy()); QVERIFY(!window.isVisible());
    const auto &results = window.projects_[detached ? 1 : 0].results; QVERIFY(results.at(3).state == State::Cancelled);
}
void GuiTest::trayExitSavesChanges() {
    QTemporaryDir directory; Window window(true); window.dataDir_ = directory.path();
    window.documents_[0].path = directory.filePath("saved.flow.json"); window.editGraph_->flow.name = "Saved on tray exit"; window.changed();
    window.smoke_ = false; QSignalSpy exiting(&window, &Window::quitRequested); window.requestQuit(); window.smoke_ = true;
    QCOMPARE(exiting.count(), 1); QVERIFY(!window.documents_[0].dirty);
    QCOMPARE(Flow::load(window.documents_[0].path).name, QString("Saved on tray exit")); QVERIFY(QFileInfo::exists(directory.filePath("workspace.json")));
}
void GuiTest::orchestrationForms() {
    Window window(true); window.show();
    const auto path = window.documents_[0].path;
    for (auto kind : {Kind::ProjectCreate, Kind::FlowRun, Kind::FlowWait}) {
        auto *button = window.findChild<QPushButton *>("addNode_" + kindKey(kind)); QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        const auto id = window.selected_; auto *form = window.editGraph_->form(id); QVERIFY(form);
        auto *combo = form->findChild<QComboBox *>("targetFlow"); auto *edit = form->findChild<QLineEdit *>("projectPath"); QVERIFY(combo); QVERIFY(edit);
        QVERIFY(form->findChildren<QPushButton *>().isEmpty()); QCOMPARE(combo->findData(path), 0);
        combo->setCurrentIndex(0); QTest::keyClicks(edit, "../${name}");
        QCOMPARE(window.editGraph_->flow.nodes.at(id).targetFlow, path); QCOMPARE(window.editGraph_->flow.nodes.at(id).projectPath, QString("../${name}"));
        window.documents_[0].flow.name = "Renamed"; window.refreshFlowChoices(); QCOMPARE(combo->currentText(), QString("Renamed"));
        window.documents_.push_back({Flow::initial(), "another.flow.json", false}); window.refreshFlowChoices(); QCOMPARE(combo->count(), 2);
        window.documents_.erase(window.documents_.begin() + 1); window.refreshFlowChoices(); QCOMPARE(combo->count(), 1);
        window.workGraph_->replace(window.editGraph_->flow); auto *workForm = window.workGraph_->form(id);
        auto *readOnly = workForm->findChild<QLineEdit *>("projectPath"); QVERIFY(readOnly->isReadOnly());
        auto missing = window.editGraph_->flow.nodes.at(id); missing.targetFlow = "missing.flow.json"; window.editGraph_->updateNode(missing);
        QCOMPARE(combo->currentData().toString(), missing.targetFlow); QVERIFY(combo->currentText().contains("未登録"));
    }
}
void GuiTest::orchestrationHistoryAndPersistence() {
    QTemporaryDir directory; Window window(true); window.dataDir_ = directory.path(); window.show();
    const auto parentPath = directory.filePath("parent.flow.json"), childPath = directory.filePath("child.flow.json");
    auto parent = Flow::initial(); Id previous = 1, id = 3;
    for (auto kind : {Kind::ProjectCreate, Kind::FlowRun, Kind::FlowWait}) {
        Node node; node.id = id; node.kind = kind; node.targetFlow = childPath; node.projectPath = "target";
        parent.nodes[id] = node; parent.edges.push_back({previous, id}); previous = id++;
    }
    parent.edges.push_back({previous, 2});
    auto target = Flow::initial(); Node node; node.id = 3; node.kind = Kind::Agent; node.model = "fixture"; node.prompt = "SHORT_DELAY";
    target.nodes[3] = node; target.edges = {{1,3},{3,2}}; parent.save(parentPath); target.save(childPath);
    window.documents_ = {{parent, parentPath, false}, {target, childPath, false}};
    window.projects_.push_back({"Parent", directory.path(), parentPath, {}}); window.selectFlow(0); window.tabs_->setCurrentIndex(1);
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    window.selectWorkNode(5); QSignalSpy done(window.runner_, &Runner::finished); window.executeFlow(); QTRY_COMPARE(done.count(), 1); QVERIFY(done[0][0].toBool());
    QCOMPARE(window.projects_.size(), size_t(2)); QCOMPARE(window.projects_[0].history.size(), size_t(5)); QCOMPARE(window.projects_[1].history.size(), size_t(3));
    QVERIFY(window.projects_[0].historyId != window.projects_[1].historyId);
    QCOMPARE(window.projects_[1].flowPath, childPath); QVERIFY(window.output_->toPlainText().contains("hello"));
    window.smoke_ = false;
    for (const auto &project : window.projects_) { const auto entries = project.history; for (const auto &entry : entries) window.recordNodeExecution(project.historyId, entry); }
    window.persist(); window.smoke_ = true;
    Window restored(true); restored.dataDir_ = directory.path(); restored.documents_.clear(); restored.projects_.clear(); restored.restore();
    QCOMPARE(restored.projects_.size(), size_t(2)); QCOMPARE(restored.projects_[0].history.size(), size_t(5)); QCOMPARE(restored.projects_[1].history.size(), size_t(3));
    restored.selectFlow(1); QCOMPARE(restored.projectList_->count(), 1); restored.selectWorkNode(3); QCOMPARE(restored.historyList_->count(), 1);
    QVERIFY(restored.output_->toPlainText().contains("hello")); restored.selectWorkNode(0); QCOMPARE(restored.historyList_->count(), 0);
}
void GuiTest::spaceZoomToFit() {
    Window window(true); window.resize(1400, 950); window.show();
    auto flow = Flow::initial(); flow.nodes[1].position = {-100000, 5000}; flow.nodes[2].position = {100000, -5000};
    Node script; script.id = 3; script.kind = Kind::Script; script.title = "Fit target"; script.position = {1000, 8000}; script.scriptPath = "transform.py";
    flow.nodes[3] = script; flow.edges = {{1, 3}, {3, 2}};
    window.editGraph_->replace(flow); window.workGraph_->replace(flow);
    for (int tab : {0, 1}) {
        window.tabs_->setCurrentIndex(tab); QCoreApplication::processEvents();
        auto *view = tab == 0 ? window.editView_ : window.workView_;
        auto *graph = tab == 0 ? window.editGraph_ : window.workGraph_;
        auto *scene = tab == 0 ? window.editScene_ : window.workScene_;
        graph->replace(flow); QCoreApplication::processEvents();
        const auto before = graph->flow.toJson(); view->setupScale(1.8); view->centerOn(-3000, 12000);
        scene->nodeGraphicsObject(1)->setSelected(true); scene->setFocusItem(nullptr); view->setFocus(); QTRY_VERIFY(view->hasFocus());
        QTest::keyClick(view, Qt::Key_Space); QCoreApplication::processEvents();
        QRectF bounds;
        for (auto *item : scene->items()) if (dynamic_cast<NodeGraphicsObject *>(item) || dynamic_cast<ConnectionGraphicsObject *>(item)) bounds = bounds.united(item->sceneBoundingRect());
        for (const auto &[id, node] : graph->flow.nodes) {
            const auto rect = view->mapFromScene(scene->nodeGraphicsObject(id)->sceneBoundingRect()).boundingRect();
            QVERIFY2(view->viewport()->rect().contains(rect), qPrintable(QString("Node %1 not fitted on tab %2").arg(id).arg(tab)));
        }
        const auto expected = qMin((view->viewport()->width() - 48.0) / bounds.width(), (view->viewport()->height() - 48.0) / bounds.height());
        QVERIFY(qAbs(view->transform().m11() - expected) < 1e-8); QVERIFY(view->transform().m11() < 0.15);
        QVERIFY(graph->flow.toJson() == before); QVERIFY(scene->nodeGraphicsObject(1)->isSelected());
        const auto transform = view->transform(); const auto center = view->mapToScene(view->viewport()->rect().center());
        QTest::keyClick(view, Qt::Key_Space); QCOMPARE(view->transform(), transform);
        QVERIFY(QLineF(center, view->mapToScene(view->viewport()->rect().center())).length() < 1.0);
        // Fit also zooms in after replacing a spread-out flow with a compact one.
        graph->replace(Flow::initial()); QCoreApplication::processEvents(); scene->setFocusItem(nullptr); view->setFocus();
        view->setupScale(0.2); QTest::keyClick(view, Qt::Key_Space); QVERIFY(view->transform().m11() > 0.2);
    }
}
void GuiTest::spaceFitFocusAndEmpty() {
    Window window(true); window.resize(1400, 950); window.show();
    auto flow = Flow::initial(); Node node; node.id = 3; node.kind = Kind::Script; node.scriptPath = "before"; flow.nodes[3] = node;
    window.editGraph_->replace(flow); window.workGraph_->replace(flow);
    for (int tab : {0, 1}) {
        window.tabs_->setCurrentIndex(tab); QCoreApplication::processEvents();
        auto *view = tab == 0 ? window.editView_ : window.workView_;
        auto *graph = tab == 0 ? window.editGraph_ : window.workGraph_;
        auto *scene = tab == 0 ? window.editScene_ : window.workScene_;
        view->setupScale(0.8); view->setFocus(); scene->setFocusItem(scene->nodeGraphicsObject(1)); QVERIFY(scene->focusItem());
        const auto transform = view->transform(); QTest::keyClick(view, Qt::Key_Space); QCOMPARE(view->transform(), transform);
        auto *edit = graph->form(3)->findChild<QLineEdit *>("scriptPath"); QVERIFY(edit); edit->setFocus(); edit->end(false); const auto beforeText = edit->text();
        QTest::keyClicks(edit, " "); QCOMPARE(view->transform(), transform); QCOMPARE(edit->text(), tab == 0 ? beforeText + " " : beforeText);
        scene->setFocusItem(nullptr); view->setFocus();
        QTest::keyClick(view, Qt::Key_Space, Qt::ControlModifier); QCOMPARE(view->transform(), transform);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->viewport()->rect().bottomRight() - QPoint(5, 5));
        QTRY_VERIFY(!scene->focusItem()); view->setupScale(0.2); QTest::keyClick(view, Qt::Key_Space); QVERIFY(view->transform().m11() > 0.2);
    }
    Graph graph; BasicGraphicsScene scene(graph); GraphView empty(&scene); empty.resize(640, 480); empty.show(); empty.setFocus();
    QCoreApplication::processEvents(); const auto transform = empty.transform(); QTest::keyClick(&empty, Qt::Key_Space); QCOMPARE(empty.transform(), transform);
}
}
QTEST_MAIN(an::GuiTest)
