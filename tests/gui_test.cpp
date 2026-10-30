#include "gui_test.h"
#include "ui/window.h"
#include "ui/nodeform.h"
#include "ui/graphview.h"
#include <QSystemTrayIcon>
#include "ui/theme.h"
#include "ui/projectopener.h"
#include "core/history.h"
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
#include <QDoubleSpinBox>
#include <QStandardPaths>
#include <QToolButton>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextDocument>
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
#include <QtNodes/internal/AbstractNodeGeometry.hpp>
#include <QtNodes/internal/UndoCommands.hpp>

namespace an {
using namespace QtNodes;
void GuiTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("AgentNodify GUI tests");
}
void GuiTest::applicationIconAsset() {
    Window window(true);
    const QIcon asset(strings::ApplicationIconResource);
    QVERIFY(!asset.isNull());
    QVERIFY(!window.windowIcon().isNull());
    for (const QSize size : {QSize(16, 16), QSize(32, 32), QSize(48, 48), QSize(256, 256)}) {
        QVERIFY(asset.availableSizes().contains(size));
        const auto image = asset.pixmap(size).toImage();
        QVERIFY(!image.isNull());
        QCOMPARE(window.windowIcon().pixmap(size).toImage(), image);
        QCOMPARE(window.trayIcon_->icon().pixmap(size).toImage(), image);
    }
}
void GuiTest::licenseViewer() {
    Window window(true);
    auto *action = window.findChild<QAction *>(strings::ShowLicensesAction);
    QVERIFY(action);
    action->trigger();
    auto *dialog = window.findChild<QDialog *>(strings::LicenseDialog);
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    auto *list = dialog->findChild<QListWidget *>(strings::LicenseList);
    auto *text = dialog->findChild<QPlainTextEdit *>(strings::LicenseText);
    auto *versions = dialog->findChild<QLabel *>(strings::LicenseVersions);
    QVERIFY(list && text && versions);
    QVERIFY(text->isReadOnly());
    QVERIFY(versions->text().contains(strings::ApplicationVersion));
    QVERIFY(versions->text().contains(QT_VERSION_STR));
    QVERIFY(versions->text().contains(qVersion()));
    QVERIFY(text->toPlainText().startsWith("MIT License"));
    QVERIFY(text->toPlainText().contains("AgentNodify contributors"));
    const auto baseCount = int(sizeof(strings::LicenseDocuments) / sizeof(strings::LicenseDocuments[0]));
    QCOMPARE(list->count(), baseCount + AGENTNODIFY_DISTRIBUTION_TEXT_COUNT);
    bool foundDmitry = false, foundNiels = false, foundPatrick = false;
    bool foundEvan = false, foundGene = false, foundException = false;
    for (int row = 0; row < list->count(); ++row) {
        list->setCurrentRow(row);
        QFile file(list->item(row)->data(Qt::UserRole).toString());
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        // QTextDocument normalizes line separators and NBSP in toPlainText().
        // Compare the complete rendered text, without changing the source file.
        QTextDocument expectedDocument;
        expectedDocument.setPlainText(QString::fromUtf8(file.readAll()));
        const auto expected = expectedDocument.toPlainText();
        QVERIFY(!expected.isEmpty());
        QCOMPARE(text->toPlainText(), expected);
        foundDmitry |= expected.contains("Dmitry Pinaev");
        foundNiels |= expected.contains("Niels Lohmann");
        foundPatrick |= expected.contains("Patrick Boettcher");
        foundEvan |= expected.contains("Evan Nemerson");
        foundGene |= expected.contains("Gene Hightower");
        foundException |= expected.contains("Exception 1:") && expected.contains("Exception 2:");
    }
    QVERIFY(foundDmitry && foundNiels && foundPatrick && foundEvan && foundGene && foundException);
    action->trigger();
    QCOMPARE(window.findChildren<QDialog *>(strings::LicenseDialog).size(), 1);
    QPointer<QDialog> guard(dialog);
    dialog->close();
    QTRY_VERIFY(guard.isNull());
    action->trigger();
    QVERIFY(window.findChild<QDialog *>(strings::LicenseDialog));
    window.findChild<QDialog *>(strings::LicenseDialog)->close();
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
    QCOMPARE(graph.form(id), form); QCOMPARE(graph.nodeData(id, NodeRole::Caption).toString(), kindName(Kind::Agent));
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
    QVERIFY(!palette->findChild<QPushButton *>("addNode_start")); QVERIFY(palette->findChild<QPushButton *>("addNode_end")); QVERIFY(palette->findChild<QPushButton *>("addNode_end")->isEnabled());
    QCOMPARE(window.editGraph_->flow.nodes.size(), size_t(1)); QVERIFY(window.editGraph_->flow.nodes.at(1).kind == Kind::Start);
    for (auto kind : {Kind::Agent, Kind::Script, Kind::Shell, Kind::All, Kind::Any, Kind::Branch}) {
        auto *button = palette->findChild<QPushButton *>("addNode_" + kindKey(kind)); QVERIFY(button); QVERIFY(button->toolTip().contains(nodeDescription(kind))); QVERIFY(button->toolTip().contains(button->text()));
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
    Window window(true); window.editGraph_->replace(Flow::initial()); window.changed(); window.show();
    auto id = window.editGraph_->addNode("agent");
    auto node = window.editGraph_->flow.nodes.at(id); node.title = "Work node"; node.effort = "medium";
    node.prompt = "line one\nline two"; window.editGraph_->updateNode(node);
    window.tabs_->setCurrentIndex(1); QCoreApplication::processEvents();
    auto *form = window.workGraph_->form(id); QVERIFY(form);
    QCOMPARE(form->node().prompt, node.prompt); QCOMPARE(form->node().effort, node.effort);
    auto *item = window.workScene_->nodeGraphicsObject(id); QVERIFY(item);
    QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsSelectable));
    QVERIFY(item->flags().testFlag(QGraphicsItem::ItemIsMovable));
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
void GuiTest::paletteWidthAndElision() {
    Window window(true); window.show(); QCoreApplication::processEvents(); auto *palette=window.findChild<QWidget *>("nodePalette"); QVERIFY(palette); QVERIFY(palette->width()>=235);
    auto *button=dynamic_cast<ElidedButton *>(palette->findChild<QPushButton *>("addNode_project_create")); QVERIFY(button); const auto full=button->text();
    window.editorSplit_->setSizes({180,900,0}); QCoreApplication::processEvents(); QCOMPARE(button->text(),full); QVERIFY(button->toolTip().contains(full));
    ElidedButton narrow("＋ AgentNodify プロジェクト作成 とても長い名前"); narrow.resize(120,40); narrow.show(); QCoreApplication::processEvents();
    QVERIFY(narrow.displayedText().endsWith("...")); QVERIFY(!narrow.displayedText().contains(QChar(0x2026))); QVERIFY(narrow.fontMetrics().horizontalAdvance(narrow.displayedText())<=narrow.width());
    narrow.resize(700,40); QCOMPARE(narrow.displayedText(),narrow.text());
    const auto before=window.editGraph_->flow.nodes.size(); QTest::mouseClick(button,Qt::LeftButton); QCOMPARE(window.editGraph_->flow.nodes.size(),before+1);
    QVERIFY(palette->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-palette-elided.png")));
}
void GuiTest::optionalTerminalControls() {
    Window window(true); window.show(); auto *add=window.findChild<QPushButton *>("addNode_end"); QVERIFY(add); QVERIFY(add->isEnabled()); QCOMPARE(window.editGraph_->flow.nodes.size(),size_t(1)); QVERIFY(window.editGraph_->flow.validate().isEmpty());
    QTest::mouseClick(add,Qt::LeftButton); const auto id=window.selected_; QVERIFY(window.editGraph_->flow.nodes.at(id).kind==Kind::End); QVERIFY(!add->isEnabled()); QCOMPARE(window.editGraph_->addNode("end"),InvalidNodeId);
    window.editScene_->clearSelection();window.editScene_->nodeGraphicsObject(id)->setSelected(true);for(auto *action:window.editView_->actions())if(action->shortcut()==QKeySequence(Qt::CTRL|Qt::Key_D))action->trigger();QCOMPARE(window.editGraph_->flow.nodes.size(),size_t(2));
    auto answer=[&](QMessageBox::StandardButton button){QTimer::singleShot(0,&window,[button]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);box->button(button)->click();});};
    window.editScene_->clearSelection(); window.editScene_->nodeGraphicsObject(id)->setSelected(true);
    window.editView_->deleteSelectionAction()->trigger(); QVERIFY(!window.editGraph_->nodeExists(id)); QVERIFY(add->isEnabled()); QVERIFY(!QApplication::activeModalWidget());
    window.stepHistory(false); QVERIFY(window.editGraph_->nodeExists(id)); QVERIFY(!add->isEnabled());
    answer(QMessageBox::Ok); window.stepHistory(true); QVERIFY(add->isEnabled());
    window.newFlow(); QCOMPARE(window.editGraph_->flow.nodes.size(),size_t(1)); QVERIFY(add->isEnabled());
    window.editScene_->nodeGraphicsObject(1)->setSelected(true); window.editView_->deleteSelectionAction()->trigger(); QVERIFY(window.editGraph_->nodeExists(1));
}
void GuiTest::timingNodeForms() {
    Window window(true); window.show(); QTest::mouseClick(window.findChild<QPushButton *>("addNode_delay"),Qt::LeftButton); const auto delay=window.selected_;
    auto *form=window.editGraph_->form(delay); auto *duration=form->findChild<QSpinBox *>("waitDuration"); auto *unit=form->findChild<QComboBox *>("waitUnit"); QVERIFY(duration); QVERIFY(!form->findChild<QDoubleSpinBox *>("waitDuration")); QVERIFY(unit); QCOMPARE(unit->count(),4); QCOMPARE(unit->currentData().toString(),QString("seconds"));
    QCOMPARE(form->width(),210);QVERIFY(!form->findChild<QLineEdit *>("nodeTitle")); duration->setValue(5);duration->setFocus();QTest::keyClick(duration,Qt::Key_Period);QVERIFY(!duration->text().contains('.'));
    unit->setCurrentIndex(unit->findData("hours")); QCOMPARE(window.editGraph_->flow.nodes.at(delay).waitUnit,QString("hours")); window.stepHistory(false); QCOMPARE(unit->currentData().toString(),QString("seconds")); window.stepHistory(true); QCOMPARE(unit->currentData().toString(),QString("hours"));
    QTest::mouseClick(window.findChild<QPushButton *>("addNode_schedule"),Qt::LeftButton); const auto schedule=window.selected_; auto *scheduled=window.editGraph_->form(schedule);
    QCOMPARE(scheduled->width(),210);QCOMPARE(scheduled->findChild<QWidget *>("scheduleWeekdays")->width(),132);QVERIFY(!scheduled->findChild<QLineEdit *>("nodeTitle"));
    for(QString field:{"scheduleMonth","scheduleDay","scheduleHour","scheduleMinute"}){auto *edit=scheduled->findChild<QLineEdit *>(field); QVERIFY(edit); QCOMPARE(edit->text(),QString("*"));}
    auto *month=scheduled->findChild<QLineEdit *>("scheduleMonth");month->setFocus();month->selectAll();QTest::keyClicks(month,"1,2"); QCOMPARE(window.editGraph_->flow.nodes.at(schedule).scheduleMonth,QString("1,2"));window.stepHistory(false);QCOMPARE(month->text(),QString("*"));window.stepHistory(true);
    for(int day=1;day<=7;++day){auto *check=scheduled->findChild<QCheckBox *>(QString("scheduleWeekday%1").arg(day));auto *label=scheduled->findChild<QLabel *>(QString("scheduleWeekdayLabel%1").arg(day));QVERIFY(check);QVERIFY(label);QVERIFY(!check->isChecked());QVERIFY(label->styleSheet().contains("#75899c"));}
    auto *monday=scheduled->findChild<QCheckBox *>("scheduleWeekday1");QTest::mouseClick(monday,Qt::LeftButton);QVERIFY(window.editGraph_->flow.nodes.at(schedule).scheduleWeekdays==std::vector<int>{1});QVERIFY(scheduled->findChild<QLabel *>("scheduleWeekdayLabel1")->styleSheet().contains("#173b5b"));
    window.stepHistory(false);QVERIFY(!monday->isChecked());window.stepHistory(true);QVERIFY(monday->isChecked());
    for(int day=1;day<=7;++day) QVERIFY(scheduled->findChild<QLabel *>(QString("scheduleWeekdayLabel%1").arg(day))->styleSheet().contains("#173b5b"));
    QTest::mouseClick(monday,Qt::LeftButton);
    for(int day=1;day<=7;++day) QVERIFY(scheduled->findChild<QLabel *>(QString("scheduleWeekdayLabel%1").arg(day))->styleSheet().contains("#75899c"));
    QTest::mouseClick(monday,Qt::LeftButton);
    for(Id id:{delay,schedule}){NodeStyle style(QJsonObject::fromVariantMap(window.editGraph_->nodeData(id,NodeRole::Style).toMap()));QCOMPARE(style.GradientColor0,QColor("#9ac9f2"));}
    window.workGraph_->replace(window.editGraph_->flow);auto *readonly=window.workGraph_->form(schedule)->findChild<QCheckBox *>("scheduleWeekday1");QVERIFY(readonly->isChecked());QVERIFY(!readonly->isEnabled());QTest::mouseClick(readonly,Qt::LeftButton);QVERIFY(readonly->isChecked());
    window.editGraph_->setNodeData(1,NodeRole::Position,QPointF(-200,100));window.editGraph_->setNodeData(delay,NodeRole::Position,QPointF(0,0));window.editGraph_->setNodeData(schedule,NodeRole::Position,QPointF(500,0));
    QCoreApplication::processEvents();QVERIFY(scheduled->findChild<QLabel *>("scheduleWeekdayLabel1")->y()>monday->y());static_cast<GraphView *>(window.editView_)->zoomToFit();QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-timing-nodes.png")));
}
void GuiTest::endlessDelayInWindow() {
    QTemporaryDir directory;Window window(true);window.show();auto flow=Flow::initial(false);Node delay;delay.id=3;delay.kind=Kind::Delay;delay.waitDuration=1;flow.nodes[3]=delay;flow.edges={{1,3},{3,3}};window.editGraph_->replace(flow);window.changed();
    window.projects_.push_back({"Endless",directory.path(),window.documents_[0].path,{}});window.refreshProjects();window.tabs_->setCurrentIndex(1);QSignalSpy done(window.runner_,&Runner::finished);window.executeFlow();QTRY_VERIFY(window.runner_->results().at(3).executions>=2);QVERIFY(done.isEmpty());QVERIFY(window.stopButton_->isEnabled());
    QTimer::singleShot(0,&window,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Ok)->click();});QTest::mouseClick(window.stopButton_,Qt::LeftButton);QTRY_COMPARE(done.count(),1);QVERIFY(!window.busy());
}
void GuiTest::editorUndoRedoParameters() {
    Window window(true); window.show(); QCoreApplication::processEvents();
    QTest::mouseClick(window.findChild<QPushButton *>("addNode_agent"),Qt::LeftButton); const auto id=window.selected_; const auto position=window.editGraph_->flow.nodes.at(id).position;
    auto stack=window.documents_[0].undo; QCOMPARE(stack->count(),1); QVERIFY(window.undoAction_->isEnabled());
    auto answer=[&](QMessageBox::StandardButton button) { QTimer::singleShot(0,&window,[button]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->windowTitle(),QString("ノードを削除")); QCOMPARE(box->standardButton(box->defaultButton()),QMessageBox::Ok);if(button==QMessageBox::Ok)QTest::keyClick(box,Qt::Key_Return);else box->button(button)->click();}); };
    answer(QMessageBox::Cancel); window.stepHistory(false); QCOMPARE(stack->index(),1); QVERIFY(window.editGraph_->nodeExists(id));
    answer(QMessageBox::Ok); window.stepHistory(false); QCOMPARE(stack->index(),0); QVERIFY(!window.editGraph_->nodeExists(id));
    window.stepHistory(true); QVERIFY(window.editGraph_->nodeExists(id)); QCOMPARE(window.editGraph_->flow.nodes.at(id).position,position);
    auto *form=window.editGraph_->form(id); auto *title=form->findChild<QLineEdit *>("nodeTitle"); QVERIFY(title); title->setFocus(); title->selectAll(); QTest::keyClicks(title,"Review");
    QCOMPARE(window.editGraph_->flow.nodes.at(id).title,QString("Review")); QCOMPARE(stack->count(),2);
    QTest::keyClick(title,Qt::Key_Z,Qt::ControlModifier); QCOMPARE(window.editGraph_->flow.nodes.at(id).title,kindName(Kind::Agent)); QCOMPARE(title->text(),kindName(Kind::Agent)); QCOMPARE(stack->index(),1);
    QTest::keyClick(title,Qt::Key_Y,Qt::ControlModifier); QCOMPARE(title->text(),QString("Review")); QCOMPARE(stack->index(),2);
    window.selectNode(id); window.showText("prompt"); const int before=stack->index(); QTest::keyClicks(window.text_,"hello world"); QCOMPARE(stack->index(),before+1);
    QTest::keyClick(window.text_,Qt::Key_Z,Qt::ControlModifier); QVERIFY(window.editGraph_->flow.nodes.at(id).prompt.isEmpty()); QVERIFY(window.text_->toPlainText().isEmpty());
    QTest::keyClick(window.text_,Qt::Key_Y,Qt::ControlModifier); QCOMPARE(window.text_->toPlainText(),QString("hello world")); QCOMPARE(window.editGraph_->flow.nodes.at(id).prompt,QString("hello world"));
    auto *check=form->findChild<QCheckBox *>("retainContext"); QVERIFY(check); QTest::mouseClick(check,Qt::LeftButton); QVERIFY(window.editGraph_->flow.nodes.at(id).retainContext);
    window.stepHistory(false); QVERIFY(!check->isChecked()); window.stepHistory(true); QVERIFY(check->isChecked());
    auto *effort=form->findChild<QComboBox *>("nodeEffort"); effort->setCurrentIndex(effort->findData("high")); window.stepHistory(false); QVERIFY(window.editGraph_->flow.nodes.at(id).effort.isEmpty()); window.stepHistory(true); QCOMPARE(effort->currentData().toString(),QString("high"));
    window.stepHistory(false); auto changed=window.editGraph_->flow.nodes.at(id); changed.model="fixture"; window.editGraph_->updateNode(changed); QVERIFY(!stack->canRedo());
    QTemporaryDir directory; window.documents_[0].path=directory.filePath("undo.json"); QVERIFY(window.saveFlow()); QVERIFY(!window.documents_[0].dirty);
    changed=window.editGraph_->flow.nodes.at(id); changed.effort="low"; window.editGraph_->updateNode(changed); QVERIFY(window.documents_[0].dirty); window.stepHistory(false); QVERIFY(!window.documents_[0].dirty);
    QTest::mouseClick(window.findChild<QPushButton *>("addNode_agent"),Qt::LeftButton); const auto allocated=window.selected_; answer(QMessageBox::Ok); window.stepHistory(false);
    QVERIFY(!window.editGraph_->nodeExists(allocated)); QVERIFY(window.documents_[0].dirty); QVERIFY(window.saveFlow()); QVERIFY(Flow::load(window.documents_[0].path).nextNodeId>allocated);
    QVERIFY(window.editGraph_->nodeExists(1)); QVERIFY(window.editGraph_->nodeExists(2));
}
void GuiTest::editorUndoRedoDeletion() {
    Window window(true); window.show(); auto flow=Flow::initial();
    for(Id id:{3u,4u}) { Node node; node.id=id; node.kind=Kind::UserInput; node.title=QString("Question %1").arg(id); node.position={double(id*300),40}; flow.nodes[id]=node; }
    flow.edges={{1,3},{3,4},{4,2},{3,3}}; window.editGraph_->replace(flow); window.changed(); auto stack=window.documents_[0].undo; stack->clear();
    int warnings=0; auto answer=[&](QMessageBox::StandardButton button) { QTimer::singleShot(0,&window,[&,button]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->windowTitle(),QString("ノードを削除")); ++warnings; box->button(button)->click();}); };
    window.editScene_->clearSelection(); window.editScene_->nodeGraphicsObject(3)->setSelected(true); window.editScene_->nodeGraphicsObject(4)->setSelected(true);
    window.editView_->setFocus(); window.editScene_->clearFocus(); QCoreApplication::processEvents(); QTest::keyClick(window.editView_,Qt::Key_Delete); QCOMPARE(stack->count(),1); QVERIFY(!window.editGraph_->nodeExists(3)); QVERIFY(!window.editGraph_->nodeExists(4)); QVERIFY(window.editGraph_->flow.edges.empty()); QCOMPARE(warnings,0); QVERIFY(!QApplication::activeModalWidget());
    window.stepHistory(false); QCOMPARE(window.editGraph_->flow.edges,flow.edges); QCOMPARE(window.editGraph_->flow.nodes.at(3).title,QString("Question 3")); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,flow.nodes.at(3).position); QCOMPARE(warnings,0);
    answer(QMessageBox::Cancel); window.stepHistory(true); QCOMPARE(stack->index(),0); QVERIFY(window.editGraph_->nodeExists(3));
    answer(QMessageBox::Ok); window.stepHistory(true); QCOMPARE(stack->index(),1); QVERIFY(!window.editGraph_->nodeExists(3)); window.stepHistory(false);
    // Edges alone, including ones attached to terminals, need no warning.
    window.editScene_->clearSelection(); auto *connection=window.editScene_->connectionGraphicsObject({1,0,3,0}); QVERIFY(connection); connection->setSelected(true);
    window.editView_->deleteSelectionAction()->trigger(); QVERIFY(!window.editGraph_->connectionExists({1,0,3,0})); QVERIFY(window.editGraph_->nodeExists(3)); QCOMPARE(warnings,2);
    window.stepHistory(false); QVERIFY(window.editGraph_->connectionExists({1,0,3,0})); QCOMPARE(warnings,2);
    window.editScene_->clearSelection(); window.editScene_->nodeGraphicsObject(1)->setSelected(true); const int count=stack->count(); window.editView_->deleteSelectionAction()->trigger(); QCOMPARE(stack->count(),count); QVERIFY(window.editGraph_->nodeExists(1));
}
void GuiTest::editorUndoIsolationAndPorts() {
    Window window(true); window.show(); auto flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; input.userChoices={"A","B","C"}; flow.nodes[3]=input;
    Node join; join.id=4; join.kind=Kind::All; join.inputs=3; flow.nodes[4]=join;
    flow.edges={{1,3},{3,4,0,0},{3,4,1,1},{3,4,2,2},{4,2}}; window.editGraph_->replace(flow); window.changed(); auto stack=window.documents_[0].undo; stack->clear();
    auto edited=input; edited.userChoices.removeAt(1); window.editGraph_->updateNode(edited,1); const auto reduced=window.editGraph_->flow.edges; QCOMPARE(stack->count(),1);
    window.stepHistory(false); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QCOMPARE(window.editGraph_->flow.edges,flow.edges); QCOMPARE(window.editGraph_->form(3)->findChildren<QLineEdit *>("choiceName").size(),3);
    window.stepHistory(true); QCOMPARE(window.editGraph_->flow.edges,reduced); QCOMPARE(window.editGraph_->flow.nodes.at(3).userChoices,edited.userChoices);
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); auto *choice=window.editGraph_->form(3)->findChildren<QLineEdit *>("choiceName").first(); choice->setFocus(); choice->selectAll(); const auto choiceIndex=stack->index(); QTest::keyClicks(choice,"Renamed"); QCOMPARE(stack->index(),choiceIndex+1);
    window.stepHistory(false); QCOMPARE(window.editGraph_->flow.nodes.at(3).userChoices,edited.userChoices); window.stepHistory(true); QCOMPARE(window.editGraph_->flow.nodes.at(3).userChoices[0],QString("Renamed"));
    const auto before=window.editGraph_->flow.nodes.at(3).position; window.editGraph_->setNodeData(3,NodeRole::Position,QPointF(500,300)); window.stepHistory(false); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,before); window.stepHistory(true); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,QPointF(500,300));
    const auto beforeNative=stack->index(); window.editScene_->undoStack().push(new QtNodes::ConnectCommand(window.editScene_,{1,0,4,0})); QCOMPARE(stack->index(),beforeNative+1); QVERIFY(window.editGraph_->connectionExists({1,0,4,0}));
    window.stepHistory(false); QVERIFY(!window.editGraph_->connectionExists({1,0,4,0})); QCoreApplication::processEvents(); QCOMPARE(window.editScene_->undoStack().count(),0);
    window.newFlow(); auto other=window.documents_[1].undo; QTest::mouseClick(window.findChild<QPushButton *>("addNode_agent"),Qt::LeftButton); QCOMPARE(other->count(),1); window.selectFlow(0); QCOMPARE(window.documents_[0].undo,stack);
    window.stepHistory(false); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,before); window.selectFlow(1); QCOMPARE(other->index(),1);
    QTimer::singleShot(0,&window,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); box->button(QMessageBox::Ok)->click();}); window.stepHistory(false); QCOMPARE(window.editGraph_->flow.nodes.size(),size_t(1));
    const auto next=window.editGraph_->addNode("agent"); QVERIFY(next>2); QVERIFY(!other->canRedo());
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
            if (enabled) menu->actions()[0]->trigger();
            menu->close();
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
    window.editGraph_->replace(Flow::initial()); window.changed();
    const auto agent = window.editGraph_->addNode("agent"), branch = window.editGraph_->addNode("branch");
    auto node = window.editGraph_->flow.nodes.at(agent); node.model = "fixture"; node.effort = "high"; node.prompt = "COUNT_TO_3";
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
    QCOMPARE(window.historyList_->item(0)->text(),QString("実行 3 継続コンテキスト\nCodex fixture High"));
    QCOMPARE(window.historyList_->item(2)->text(),QString("実行 1\nCodex fixture High"));
    QVERIFY(window.historyList_->item(0)->data(Qt::UserRole+1).toString().contains("color:#929caa"));
    const auto promptLabels=window.log_->findChildren<QLabel *>("agentInputPrompt"); QCOMPARE(promptLabels.size(),1); QCOMPARE(promptLabels[0]->text(),node.prompt);
    QVERIFY(window.output_->toPlainText().contains('3'));
    window.historyList_->setCurrentRow(2); const auto firstId = window.selectedHistory_;
    QVERIFY(!window.log_->toPlainText().contains("--- 実行"));
    QCOMPARE(window.historyList_->item(2)->text(),QString("実行 1\nCodex fixture High"));
    auto editedNode=node; editedNode.model="other-model"; editedNode.effort="low"; window.editGraph_->updateNode(editedNode); window.refreshWorkHistory();
    QCOMPARE(window.historyList_->item(2)->text(),QString("実行 1\nCodex fixture High")); window.editGraph_->updateNode(node);
    QVERIFY(!window.historyList_->item(2)->text().contains(node.title));
    QVERIFY(window.output_->toPlainText().contains('1')); QVERIFY(window.historyList_->item(2)->toolTip().contains(QDate::currentDate().toString("yyyy-MM-dd")));
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
    original.smoke_ = false; original.recordNodeExecution(original.projects_[0].historyId, entry); original.persist(); original.smoke_ = true;
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
void GuiTest::historyLabelFormat() {
    Window window(true); window.show(); window.projects_.push_back({"test",".",window.documents_[0].path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1);
    NodeExecution entry; entry.id=QUuid::createUuid().toString(QUuid::WithoutBraces); entry.node=1; entry.agent="Codex"; entry.model="gpt-6-luna"; entry.executionNumber=1; entry.retainContext=true;
    entry.startedAt=QDateTime::currentDateTimeUtc(); entry.state=State::Succeeded;
    window.projects_[0].history={entry}; window.selectWorkNode(1);
    QCOMPARE(window.historyList_->item(0)->text(),QString("実行 1\nCodex gpt-6-luna Default"));
    auto refresh=[&] { window.projects_[0].history[0]=entry; window.refreshWorkHistory(); };
    entry.executionNumber=2; entry.contextResumed=true; refresh();
    QCOMPARE(window.historyList_->item(0)->text(),QString("実行 2 継続コンテキスト\nCodex gpt-6-luna Default"));
    entry.retainContext=false; entry.contextResumed=false; entry.model="gpt-6-sol"; entry.effort="high"; refresh();
    QCOMPARE(window.historyList_->item(0)->text(),QString("実行 2\nCodex gpt-6-sol High"));
    entry.model="<b>literal & model</b>"; refresh();
    const auto html=window.historyList_->item(0)->data(Qt::UserRole+1).toString(); QVERIFY(html.contains("&lt;b&gt;literal &amp; model&lt;/b&gt;")); QVERIFY(!html.contains("継続コンテキスト"));
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
    window.recordNodeExecution(window.projects_[0].historyId, execution);
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
    window.editGraph_->replace(Flow::initial()); window.changed();
    auto *add = window.findChild<QPushButton *>("addNode_shell"); QVERIFY(add); QTest::mouseClick(add, Qt::LeftButton);
    const auto id = window.selected_; QVERIFY(window.editGraph_->flow.nodes.at(id).kind == Kind::Shell);
    auto *form = window.editGraph_->form(id); QVERIFY(form); auto *preview = form->findChild<TextPreview *>("shellCommandPreview"); QVERIFY(preview);
    auto *mode=form->findChild<QComboBox *>("shellOutput"); QVERIFY(mode); QCOMPARE(mode->count(),2); QCOMPARE(mode->currentData().toString(),QString("json"));
    mode->setCurrentIndex(mode->findData("text")); QCOMPARE(window.editGraph_->flow.nodes.at(id).shellOutput,QString("text")); QCOMPARE(Flow::fromJson(window.editGraph_->flow.toJson()).nodes.at(id).shellOutput,QString("text"));
    window.stepHistory(false); QCOMPARE(mode->currentData().toString(),QString("json")); window.stepHistory(true); QCOMPARE(mode->currentData().toString(),QString("text")); window.stepHistory(false);
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
    auto *workMode=workForm->findChild<QComboBox *>("shellOutput"); QVERIFY(workMode); QTest::keyClick(workMode,Qt::Key_Down); QCOMPARE(workMode->currentData().toString(),QString("json"));
    QSignalSpy done(window.runner_, &Runner::finished); window.executeFlow(); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 15000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    const auto stdoutText = QString::fromStdString(window.runner_->results().at(2).output.at("stdout")).trimmed();
    QCOMPARE(QDir::fromNativeSeparators(stdoutText), QFileInfo(directory.filePath("flows")).canonicalFilePath());
    window.tabs_->setCurrentIndex(0); mode->setCurrentIndex(mode->findData("text")); window.tabs_->setCurrentIndex(1); window.executeFlow(); QTRY_COMPARE_WITH_TIMEOUT(done.count(),2,15000); QVERIFY(done[1][0].toBool());
    QVERIFY(window.runner_->results().at(2).output.is_string()); QCOMPARE(QDir::fromNativeSeparators(QString::fromStdString(window.runner_->results().at(2).output.get<std::string>()).trimmed()),QFileInfo(directory.filePath("flows")).canonicalFilePath());
    bool textHistory=false; for(auto entry=window.projects_[0].history.rbegin();entry!=window.projects_[0].history.rend();++entry) if(entry->node==id) { QVERIFY(!entry->structuredOutput); textHistory=true; break; } QVERIFY(textHistory);
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
void GuiTest::interactionWhileWaiting() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow=Flow::initial();
    Node input; input.id=3; input.kind=Kind::UserInput; flow.nodes[3]=input;
    Node agent; agent.id=4; agent.kind=Kind::Agent; agent.model="fixture"; agent.prompt="RESPONSE_TWO"; flow.nodes[4]=agent; flow.edges={{1,3},{3,4},{4,2}};
    const auto path=directory.filePath("main.flow.json"); window.documents_[0]={flow,path,true}; window.selectFlow(0);
    for(QString name:{"First","Second"}) {const auto folder=directory.filePath(name); QVERIFY(QDir().mkpath(folder)); window.projects_.push_back({name,folder,path,{}});}
    window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.config_.codex=QCoreApplication::applicationDirPath()+"/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex+=".exe";
#endif
    QSignalSpy primary(window.runner_,&Runner::finished); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); const auto first=window.inputPrompts_[0].request.id;
    window.inputText_->setPlainText("draft stays"); QVERIFY(window.activities_->isEnabled()); QVERIFY(window.projectList_->isEnabled()); QVERIFY(window.tabs_->isTabEnabled(0)); QVERIFY(!window.runButton_->isEnabled());
    window.projectList_->setCurrentRow(1); QCOMPARE(window.project_,1); QVERIFY(window.runButton_->isEnabled()); QVERIFY(window.inputPanel_->isHidden()); QVERIFY(window.inputText_->toPlainText().isEmpty()); QVERIFY(!window.stopButton_->isEnabled());
    auto dismiss=[&] { QTimer::singleShot(0,&window,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->windowTitle(),QString("実行中のフロー")); box->button(QMessageBox::Ok)->click();}); };
    window.tabs_->setCurrentIndex(0); auto edited=window.editGraph_->flow.nodes.at(4); edited.prompt="Say hello"; dismiss(); window.editGraph_->updateNode(edited);
    QVERIFY(window.saveFlow()); QCOMPARE(Flow::load(path).nodes.at(4).prompt,QString("RESPONSE_TWO")); QVERIFY(window.coordinator_->running(window.projects_[0].historyId)); QVERIFY(window.projects_[0].results.at(3).state==State::Running);
    dismiss(); QVERIFY(!window.editGraph_->deleteNode(3)); QCOMPARE(window.inputPrompts_[0].request.id,first);
    window.selectProject(0); QVERIFY(window.workGraph_->nodeExists(3)); QVERIFY(window.workGraph_->flow.nodes.at(4).prompt=="RESPONSE_TWO");
    QVERIFY(window.editGraph_->setNodeData(3,NodeRole::Position,QPointF(50,200))); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,QPointF(50,200));
    bool settingsOpened=false; QTimer::singleShot(0,&window,[&]{auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog); settingsOpened=true; dialog->reject();}); window.settings(); QVERIFY(settingsOpened);
    window.newFlow(); QCOMPARE(window.documents_.size(),size_t(2)); QCOMPARE(window.current_,1); QCOMPARE(window.inputPrompts_[0].request.id,first);
    window.documents_[1].path=directory.filePath("unused.flow.json"); window.documents_[1].dirty=false;
    QTimer::singleShot(0,&window,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click();}); window.removeFlow(1); QCOMPARE(window.documents_.size(),size_t(1)); QVERIFY(window.coordinator_->running(window.projects_[0].historyId));
    window.selectFlow(0); window.selectProject(0); QVERIFY(window.workGraph_->flow.nodes.at(4).prompt=="RESPONSE_TWO");
    window.selectProject(1); window.tabs_->setCurrentIndex(1); QVERIFY(window.runButton_->isEnabled()); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(2));
    QString second; for(const auto &prompt:window.inputPrompts_) if(prompt.projectId==window.projects_[1].historyId) second=prompt.request.id;
    QVERIFY(!second.isEmpty()); QVERIFY(window.inputText_->toPlainText().isEmpty()); QVERIFY(window.coordinator_->running(window.projects_[0].historyId)); QVERIFY(window.coordinator_->running(window.projects_[1].historyId));
    QVERIFY(window.coordinator_->submitUserInput(second,"second",0)); QTRY_VERIFY(!window.coordinator_->running(window.projects_[1].historyId)); QCOMPARE(primary.count(),0); QVERIFY(window.busy()); QVERIFY(window.runButton_->isEnabled());
    QVERIFY(window.projects_[1].results.at(2).output=="two"); QCOMPARE(window.project_,1); QVERIFY(window.inputPanel_->isHidden());
    window.selectProject(0); QCOMPARE(window.inputText_->toPlainText(),QString("draft stays")); window.selectProject(1);
    QVERIFY(window.coordinator_->submitUserInput(first,"first",0)); QTRY_COMPARE(primary.count(),1); QVERIFY(primary[0][0].toBool()); QVERIFY(!window.busy()); QCOMPARE(window.project_,1);
    QVERIFY(window.projects_[0].results.at(2).output=="two"); QVERIFY(window.projects_[1].results.at(2).output=="two"); QCOMPARE(window.projects_[0].history.size(),size_t(4)); QCOMPARE(window.projects_[1].history.size(),size_t(4));
    window.selectWorkNode(4); QVERIFY(window.output_->toPlainText().contains("two"));
}
void GuiTest::compactNamelessNodes() {
    Graph graph; graph.replace(Flow::initial());
    const auto agent=graph.addNode("agent"); auto named=graph.flow.nodes.at(agent); named.title="Custom agent"; graph.updateNode(named);
    QCOMPARE(graph.nodeData(agent,NodeRole::Caption).toString(),kindName(Kind::Agent)); QCOMPARE(graph.form(agent)->findChild<QLineEdit *>("nodeTitle")->text(),named.title);
    for(Kind kind:{Kind::All,Kind::Any,Kind::Branch,Kind::UserInput,Kind::Delay,Kind::Schedule}) {
        const auto id=graph.addNode(kindKey(kind)); auto *form=graph.form(id); QVERIFY(form); form->show(); QCoreApplication::processEvents(); QCOMPARE(form->width(),kind==Kind::All || kind==Kind::Any ? 110 : 210);
        if(kind==Kind::All || kind==Kind::Any) for(auto *label:form->findChildren<QLabel *>()) QVERIFY(label->text()!=QString::fromUtf8("入力ポート数"));
        QCOMPARE(graph.nodeData(id,NodeRole::Caption).toString(),kindName(kind));
        if(kind!=Kind::UserInput) { QVERIFY(!form->findChild<QLineEdit *>("nodeTitle")); QVERIFY(graph.flow.nodes.at(id).title.isEmpty()); auto node=graph.flow.nodes.at(id); node.title="Old name"; graph.updateNode(node); QVERIFY(graph.flow.nodes.at(id).title.isEmpty()); }
        for(auto *edit:form->findChildren<QLineEdit *>()) QVERIFY2(edit->geometry().right()<=edit->parentWidget()->width(),qPrintable(edit->objectName()));
    }
    auto saved=graph.flow.toJson(); for(auto &node:saved["nodes"]) if(!hasNodeName(parseKind(node["kind"]))) node["title"]="legacy name";
    const auto restored=Flow::fromJson(saved); for(const auto &[id,node]:restored.nodes) if(!hasNodeName(node.kind)) QVERIFY(node.title.isEmpty());
}
void GuiTest::nodeColorsAndPalette() {
    Window window(true); window.show(); int i=0;
    for(const auto &[kind,color]:std::vector<std::pair<Kind,QColor>>{{Kind::All,QColor("#164b86")},{Kind::Any,QColor("#164b86")},{Kind::Branch,QColor("#e59a42")},{Kind::UserInput,QColor("#6f2635")},{Kind::Delay,QColor("#9ac9f2")},{Kind::Schedule,QColor("#9ac9f2")},{Kind::End,QColor("#efc65e")}}) {
        const auto id=window.editGraph_->addNode(kindKey(kind)); QVERIFY(id!=InvalidNodeId); window.editGraph_->setNodeData(id,NodeRole::Position,QPointF((i%3)*360,(i/3)*280));++i;
        NodeStyle style(QJsonObject::fromVariantMap(window.editGraph_->nodeData(id,NodeRole::Style).toMap()));QCOMPARE(style.GradientColor0,color);QCOMPARE(style.FontColor,nodeForeground(kind));QCOMPARE(style.NormalBoundaryColor,nodeAccent(kind));QVERIFY(style.NormalBoundaryColor!=style.GradientColor0);
        auto *button=window.findChild<QPushButton *>("addNode_"+kindKey(kind));QVERIFY(button);QVERIFY(button->styleSheet().contains("background:"+color.name()));
        if(kind==Kind::End)QVERIFY(!button->isEnabled());
    }
    window.editGraph_->setNodeData(1,NodeRole::Position,QPointF(-120,100));QCoreApplication::processEvents();static_cast<GraphView *>(window.editView_)->zoomToFit();QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-node-colors.png")));
}
void GuiTest::runningFlowEditRestrictions() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow=Flow::initial();
    Node input; input.id=3; input.kind=Kind::UserInput; input.title="Question"; input.position={200,40}; flow.nodes[3]=input; flow.edges={{1,3},{3,2}};
    window.editGraph_->replace(flow); window.changed(); const auto path=window.documents_[0].path;
    window.projects_.push_back({"Running",directory.path(),path,{}}); window.projects_.push_back({"Idle",directory.path(),path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1);
    window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); window.selectProject(1); window.tabs_->setCurrentIndex(0); QVERIFY(window.currentFlowRunning());
    int warnings=0; auto reject=[&] { QTimer::singleShot(0,&window,[&]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->windowTitle(),QString("実行中のフロー")); ++warnings; box->button(QMessageBox::Ok)->click();}); };
    const auto before=window.editGraph_->flow.toJson();
    reject(); QTest::mouseClick(window.findChild<QPushButton *>("addNode_agent"),Qt::LeftButton); QCOMPARE(window.editGraph_->flow.toJson(),before);
    auto *edit=window.editGraph_->form(3)->findChild<QLineEdit *>("nodeTitle"); QVERIFY(edit); reject(); QTest::mouseClick(edit,Qt::LeftButton); QCOMPARE(edit->text(),QString("Question"));
    reject(); window.editGraph_->addConnection({1,0,2,0}); QCOMPARE(window.editGraph_->flow.toJson(),before);
    reject(); QVERIFY(!window.editGraph_->deleteConnection({1,0,3,0})); QCOMPARE(window.editGraph_->flow.toJson(),before);
    reject(); QVERIFY(!window.editGraph_->deleteNode(3)); QCOMPARE(warnings,5);
    static_cast<GraphView *>(window.editView_)->zoomToFit();
    const auto port=window.editView_->mapFromScene(window.editScene_->nodeGeometry().portScenePosition(3,PortType::Out,0,window.editScene_->nodeGraphicsObject(3)->sceneTransform()));
    reject(); QTest::mouseClick(window.editView_->viewport(),Qt::LeftButton,Qt::NoModifier,port); QCOMPARE(warnings,6); QCOMPARE(window.editGraph_->flow.toJson(),before);
    QVERIFY(window.editGraph_->setNodeData(3,NodeRole::Position,QPointF(350,100))); QCOMPARE(warnings,6);
    window.stepHistory(false); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,input.position); const auto historyIndex=window.documents_[0].undo->index();
    reject(); window.stepHistory(false); QCOMPARE(window.documents_[0].undo->index(),historyIndex); QCOMPARE(warnings,7); window.stepHistory(true); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,QPointF(350,100));
    window.tabs_->setCurrentIndex(1); window.selectProject(0);
    const auto original=window.projects_[0].runFlow->nodes.at(3).position;
    auto *node=window.workScene_->nodeGraphicsObject(3); QVERIFY(node); QVERIFY(node->flags().testFlag(QGraphicsItem::ItemIsMovable));
    static_cast<GraphView *>(window.workView_)->zoomToNode(3); QCoreApplication::processEvents();
    const auto start=window.workView_->mapFromScene(node->mapToScene(QPointF(node->boundingRect().width()/2,12)));
    QTest::mousePress(window.workView_->viewport(),Qt::LeftButton,Qt::NoModifier,start); QTest::mouseMove(window.workView_->viewport(),start+QPoint(60,45),20); QTest::mouseRelease(window.workView_->viewport(),Qt::LeftButton,Qt::NoModifier,start+QPoint(60,45));
    const auto moved=window.workGraph_->flow.nodes.at(3).position; QVERIFY(moved!=original);
    QCOMPARE(window.projects_[0].runFlow->nodes.at(3).position,original); QCOMPARE(window.editGraph_->flow.nodes.at(3).position,QPointF(350,100));
    window.selectProject(1); window.selectProject(0); QCOMPARE(window.workGraph_->flow.nodes.at(3).position,moved);
    window.newFlow(); QVERIFY(!window.currentFlowRunning()); QVERIFY(window.editGraph_->addNode("agent")!=InvalidNodeId);
    window.selectFlow(0); QVERIFY(window.currentFlowRunning()); window.coordinator_->cancelAll(); QVERIFY(!window.currentFlowRunning()); QVERIFY(window.editGraph_->deleteNode(3));
}
void GuiTest::startAndStopDialogs() {
    QTemporaryDir directory; Window window(true); window.dataDir_=directory.path(); auto flow=Flow::initial();
    Node node; node.id=3; node.kind=Kind::Agent; node.model="fixture"; node.prompt="SLEEP"; flow.nodes[3]=node; flow.edges={{1,3},{3,2}};
    window.documents_[0]={flow,directory.filePath("main.json"),false}; window.selectFlow(0); window.projects_.push_back({"test",directory.path(),window.documents_[0].path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.show();
    window.config_.codex=QCoreApplication::applicationDirPath()+"/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex+=".exe";
#endif
    window.smoke_=false; QStringList dialogs; QMessageBox::StandardButton answer=QMessageBox::Cancel;
    QTimer responder; responder.setInterval(5); connect(&responder,&QTimer::timeout,&window,[&] { if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { dialogs<<box->windowTitle(); if(box->windowTitle()=="フローを停止" && answer==QMessageBox::Ok){QCOMPARE(box->standardButton(box->defaultButton()),QMessageBox::Ok);QTest::keyClick(box,Qt::Key_Return);}else box->done(answer); } }); responder.start();
    window.executeFlow(); QTRY_VERIFY(window.runner_->running()); QTest::qWait(40); QVERIFY(dialogs.isEmpty());
    window.tabs_->setCurrentIndex(0); auto changed=window.editGraph_->flow.nodes.at(3); changed.prompt="changed"; answer=QMessageBox::Ok; window.editGraph_->updateNode(changed); QCOMPARE(window.editGraph_->flow.nodes.at(3).prompt,QString("SLEEP")); QCOMPARE(dialogs,QStringList{"実行中のフロー"}); dialogs.clear();
    window.tabs_->setCurrentIndex(1); answer=QMessageBox::Cancel; QTest::mouseClick(window.stopButton_,Qt::LeftButton); QVERIFY(window.busy()); QCOMPARE(dialogs,QStringList{"フローを停止"}); dialogs.clear();
    answer=QMessageBox::Ok; QTest::mouseClick(window.stopButton_,Qt::LeftButton); QTRY_VERIFY(!window.busy()); QTest::qWait(40); QCOMPARE(dialogs,QStringList{"フローを停止"}); responder.stop(); window.smoke_=true;
}
void GuiTest::addedNodeAutoFit() {
    Window window(true); window.show(); QCoreApplication::processEvents(); auto *view=static_cast<GraphView *>(window.editView_);
    view->setSceneRect(-100000,-100000,200000,200000); view->centerOn(QPointF(9000,9000));
    QTest::mouseClick(window.findChild<QPushButton *>("addNode_user_input"),Qt::LeftButton); const auto id=window.selected_; QVERIFY(window.editGraph_->nodeExists(id));
    QTRY_VERIFY(view->mapToScene(view->viewport()->rect()).boundingRect().contains(window.editScene_->nodeGraphicsObject(id)->sceneBoundingRect()));
    for(const auto &[nodeId,node]:window.editGraph_->flow.nodes) QVERIFY(view->mapToScene(view->viewport()->rect()).boundingRect().contains(window.editScene_->nodeGraphicsObject(nodeId)->sceneBoundingRect()));
    view->setTransform(QTransform::fromScale(0.2,0.2)); view->setSceneRect(-10000,-10000,20000,20000); view->centerOn(QPointF(0,0)); const auto transform=view->transform();
    QTest::mouseClick(window.findChild<QPushButton *>("addNode_all"),Qt::LeftButton); QTest::qWait(30); QCOMPARE(view->transform(),transform);
}
void GuiTest::nodePlacementFromLastInteraction() {
    Window window(true); window.show(); QCoreApplication::processEvents(); auto *view=static_cast<GraphView *>(window.editView_);
    qreal right=-1e9; for(auto *item:window.editScene_->items()) if(dynamic_cast<NodeGraphicsObject *>(item)||dynamic_cast<ConnectionGraphicsObject *>(item)) right=qMax(right,item->sceneBoundingRect().right());
    auto add=[&] { QTest::mouseClick(window.findChild<QPushButton *>("addNode_user_input"),Qt::LeftButton); QCoreApplication::processEvents(); return window.selected_; };
    const auto first=add(); QVERIFY(window.editGraph_->flow.nodes.at(first).position.x()>right);
    const auto step=2*QFontMetrics(view->font()).lineSpacing(); const auto firstPosition=window.editGraph_->flow.nodes.at(first).position;
    const auto second=add(); QCOMPARE(window.editGraph_->flow.nodes.at(second).position,firstPosition+QPointF(step,step));
    // A background click is an exact scene anchor, independent of view scale/pan.
    view->setSceneRect(-10000,-10000,20000,20000); view->setTransform(QTransform::fromScale(0.7,0.7)); view->centerOn(QPointF(-1500,-800));
    const QPoint background(120,150); QVERIFY(!view->itemAt(background)); const auto anchor=view->mapToScene(background);
    QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,background); const auto third=add(); QCOMPARE(window.editGraph_->flow.nodes.at(third).position,anchor);
    // Drag on the background records its final scene location.
    view->setSceneRect(-10000,-10000,20000,20000); view->centerOn(QPointF(-3000,-2000));
    QTest::mousePress(view->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(100,100)); QTest::mouseMove(view->viewport(),QPoint(160,140),20); QTest::mouseRelease(view->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(160,140));
    const auto backgroundEnd=view->mapToScene(QPoint(160,140)); const auto fourth=add(); QCOMPARE(window.editGraph_->flow.nodes.at(fourth).position,backgroundEnd);
    // Node clicks use the node's origin, not the click position within the card.
    view->zoomToNode(first); QCoreApplication::processEvents(); auto *item=window.editScene_->nodeGraphicsObject(first); item->setZValue(100);
    auto point=view->mapFromScene(item->mapToScene(QPointF(item->boundingRect().width()/2,12))); QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,point);
    const auto fifth=add(); QCOMPARE(window.editGraph_->flow.nodes.at(fifth).position,firstPosition+QPointF(step,step));
    view->zoomToNode(first); QCoreApplication::processEvents(); point=view->mapFromScene(item->mapToScene(QPointF(item->boundingRect().width()/2,12)));
    QTest::mousePress(view->viewport(),Qt::LeftButton,Qt::NoModifier,point); QTest::mouseMove(view->viewport(),point+QPoint(70,55),20); QTest::mouseRelease(view->viewport(),Qt::LeftButton,Qt::NoModifier,point+QPoint(70,55));
    const auto moved=window.editGraph_->flow.nodes.at(first).position; QVERIFY(moved!=firstPosition); const auto sixth=add(); QCOMPARE(window.editGraph_->flow.nodes.at(sixth).position,moved+QPointF(step,step));
    const auto remembered=window.documents_[0].creationAnchor; window.newFlow(); const auto startRight=window.editScene_->nodeGraphicsObject(1)->sceneBoundingRect().right(); const auto other=add(); QVERIFY(window.editGraph_->flow.nodes.at(other).position.x()>startRight); window.selectFlow(0);
    const auto seventh=add(); QCOMPARE(window.editGraph_->flow.nodes.at(seventh).position,*remembered+QPointF(step,step));
}
void GuiTest::userInputControlEnter() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; input.userChoices={"OK","Cancel"}; flow.nodes[3]=input;
    Node first; first.id=4; first.kind=Kind::Branch; first.rules.clear(); flow.nodes[4]=first; first.id=5; flow.nodes[5]=first;
    flow.edges={{1,3},{3,4,0,0},{3,5,1,0},{4,2},{5,2}}; window.editGraph_->replace(flow); window.changed();
    window.projects_.push_back({"input",directory.path(),window.documents_[0].path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1));
    window.inputText_->setFocus(); window.inputText_->setPlainText("answer"); QTest::keyClick(window.inputText_,Qt::Key_Return); QCOMPARE(window.inputText_->toPlainText(),QString("\nanswer")); QVERIFY(window.busy());
    QVERIFY(window.inputShortcutHint_); QVERIFY(window.inputShortcutHint_->isHidden());
    QTest::keyPress(window.inputText_,Qt::Key_Control); QVERIFY(window.inputShortcutHint_->isVisible()); QCOMPARE(window.inputShortcutHint_->text(),QString("+Enterで OK"));
    QCoreApplication::processEvents(); QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-user-input-shortcut.png")));
    QTest::keyRelease(window.inputText_,Qt::Key_Control); QVERIFY(window.inputShortcutHint_->isHidden());
    window.inputSelector_->setFocus(); QTest::keyClick(window.inputSelector_,Qt::Key_Return,Qt::ControlModifier); QVERIFY(window.busy());
    window.inputText_->setFocus(); window.inputText_->setPlainText("日本語\nanswer"); QSignalSpy done(window.runner_,&Runner::finished);
    QTest::keyClick(window.inputText_,Qt::Key_Return,Qt::ControlModifier); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(window.runner_->results().at(2).output=="日本語\nanswer");
    QVERIFY(window.runner_->results().at(4).state==State::Succeeded); QVERIFY(window.runner_->results().at(5).state==State::Skipped); QVERIFY(window.inputPanel_->isHidden());
    // Hint uses the current request's first choice, and hides on deactivation.
    flow.nodes[3].userChoices[0]="<Send>"; window.editGraph_->replace(flow); window.changed(); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); window.inputText_->setFocus();
    QTest::keyPress(window.inputText_,Qt::Key_Control); QCOMPARE(window.inputShortcutHint_->text(),QString("+Enterで <Send>")); QCOMPARE(window.inputShortcutHint_->textFormat(),Qt::PlainText);
    QEvent deactivate(QEvent::WindowDeactivate); QApplication::sendEvent(&window,&deactivate); QVERIFY(window.inputShortcutHint_->isHidden()); QTest::keyRelease(window.inputText_,Qt::Key_Control); window.coordinator_->cancelAll();
}
void GuiTest::removeProjectsWhileWaiting() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; flow.nodes[3]=input; flow.edges={{1,3},{3,2}};
    window.editGraph_->replace(flow); window.changed(); const auto path=window.documents_[0].path;
    window.projects_.push_back({"Waiting",directory.path(),path,{}}); window.projects_.push_back({"Unused",directory.path(),path,{}}); window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1));
    window.inputText_->setPlainText("kept"); auto *idleRemove=window.projectList_->itemWidget(window.projectList_->item(1))->findChild<QToolButton *>("removeProject"); QVERIFY(idleRemove); QTest::mouseClick(idleRemove,Qt::LeftButton);
    QCOMPARE(window.projects_.size(),size_t(1)); QVERIFY(window.busy()); QCOMPARE(window.inputText_->toPlainText(),QString("kept"));
    auto click=[&](QMessageBox::StandardButton answer){QTimer::singleShot(0,&window,[answer]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); box->button(answer)->click();}); auto *remove=window.projectList_->itemWidget(window.projectList_->item(0))->findChild<QToolButton *>("removeProject"); QVERIFY(remove); QTest::mouseClick(remove,Qt::LeftButton);};
    click(QMessageBox::No); QCOMPARE(window.projects_.size(),size_t(1)); QVERIFY(window.busy()); QCOMPARE(window.inputPrompts_.size(),size_t(1));
    click(QMessageBox::Yes); QVERIFY(window.projects_.empty()); QVERIFY(!window.busy()); QVERIFY(window.inputPrompts_.empty()); QVERIFY(QFileInfo(directory.path()).isDir());
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
        auto *row=run->parentWidget(); row->layout()->activate(); auto *name=row->findChild<QLabel *>("projectName"); QVERIFY(name); QVERIFY(name->width()>0);
        QCOMPARE(!run->isHidden(),running); QCOMPARE(!input->isHidden(),waiting);
        if(running) QVERIFY(name->geometry().right()<run->geometry().left());
        if(waiting) QVERIFY(name->geometry().right()<input->geometry().left());
        if(running && waiting) QVERIFY(run->geometry().right()<input->geometry().left());
        QVERIFY(name->geometry().right()<row->findChild<QToolButton *>("projectOpenMenu")->geometry().left());
        QVERIFY(run->styleSheet().contains("background:transparent")); QVERIFY(input->styleSheet().contains("background:transparent"));
        if(running) { const auto image=run->pixmap().toImage(); QCOMPARE(image.pixelColor(0,0).alpha(),0); QCOMPARE(image.pixelColor(5,9).alpha(),0); }
        if(waiting) { const auto image=input->pixmap().toImage(); QCOMPARE(image.pixelColor(0,0).alpha(),0); QCOMPARE(image.pixelColor(5,6).alpha(),0); }
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
    auto nameWidth=[&] { auto *row=icon(0,"projectRunningIcon")->parentWidget(); row->layout()->activate(); return row->findChild<QLabel *>("projectName")->width(); };
    const int idleWidth=nameWidth();
    window.config_.codex = QCoreApplication::applicationDirPath() + "/fake_agent";
#ifdef Q_OS_WIN
    window.config_.codex += ".exe";
#endif
    window.executeFlow(); QTRY_VERIFY(window.runner_->results().at(3).state==State::Running); verify(0,true,false); verify(1,false,false);
    QCOMPARE(nameWidth(),idleWidth-24);
    QVERIFY(window.coordinator_->running(window.projects_[0].historyId)); QVERIFY(!window.coordinator_->running(window.projects_[1].historyId));
    window.coordinator_->cancelAll(); QTRY_VERIFY(!window.busy()); verify(0,false,false);
    QCOMPARE(nameWidth(),idleWidth);
    flow=Flow::initial(); Node input; input.id=3; input.kind=Kind::UserInput; flow.nodes[3]=input; flow.edges={{1,3},{3,2}};
    window.editGraph_->replace(flow); window.changed(); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(1)); verify(0,true,true); verify(1,false,false);
    QCOMPARE(nameWidth(),idleWidth-48);
    QVERIFY(window.coordinator_->start(window.coordinator_->projects[1])); QTRY_COMPARE(window.inputPrompts_.size(),size_t(2)); verify(0,true,true); verify(1,true,true); verify(2,false,false);
    QVERIFY(window.projectList_->isEnabled()); QVERIFY(window.projectList_->grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-project-status-icons.png")));
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
    Window window(true); window.editGraph_->replace(Flow::initial()); window.changed(); window.show(); auto *button = window.findChild<QPushButton *>("addNode_user_input"); QVERIFY(button); QTest::mouseClick(button,Qt::LeftButton);
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
        window.editGraph_->states[id] = state; NodeStyle style(QJsonObject::fromVariantMap(window.editGraph_->nodeData(id,NodeRole::Style).toMap())); QCOMPARE(style.GradientColor0,QColor("#6f2635")); QCOMPARE(style.FontColor,QColor("#f6e3e7"));
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
    QCoreApplication::processEvents();
    QVERIFY(window.inputText_->geometry().top()-window.inputMessage_->geometry().bottom()<=5);
    for(auto *label:window.inputPanel_->findChildren<QLabel *>()) QVERIFY(label->text()!=QString("ユーザー入力待ち"));
    const QString answer = "日本語の回答\n{\"value\":42}"; window.inputText_->setPlainText(answer);
    QVERIFY(!window.close()); QVERIFY(window.busy()); window.showWindow(); QCOMPARE(window.inputText_->toPlainText(),answer);
    window.focusUserInputNode(); QCoreApplication::processEvents(); QVERIFY(window.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-user-input-work.png")));
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
    QCOMPARE(window.inputSelector_->count(),1); const auto firstId = window.inputSelector_->currentData().toString(); window.inputText_->setPlainText("draft one");
    window.selectFlow(1); window.selectProject(1); QCOMPARE(window.inputSelector_->count(),1); const auto secondId = window.inputSelector_->currentData().toString();
    QVERIFY(firstId != secondId); window.inputText_->setPlainText("draft two"); window.selectFlow(0); window.selectProject(0); QCOMPARE(window.inputText_->toPlainText(),QString("draft one"));
    QString parentRequest, childRequest;
    for (const auto &prompt : window.inputPrompts_) { if (prompt.projectId == window.projects_[0].historyId) parentRequest=prompt.request.id; else childRequest=prompt.request.id; }
    QVERIFY(!parentRequest.isEmpty()); QVERIFY(!childRequest.isEmpty());
    QVERIFY(window.coordinator_->submitUserInput(parentRequest,"parent answer",0)); QTRY_COMPARE(done.count(),1); QVERIFY(done[0][0].toBool()); QVERIFY(window.busy()); QVERIFY(window.runButton_->isEnabled());
    QCOMPARE(window.inputPrompts_.size(),size_t(1)); QVERIFY(window.inputPanel_->isHidden()); QCOMPARE(window.inputSelector_->count(),0); QVERIFY(!window.stopButton_->isEnabled());
    window.selectFlow(1); window.selectProject(1); QCOMPARE(window.inputSelector_->currentData().toString(),childRequest); QCOMPARE(window.inputSelector_->currentText(),kindName(Kind::UserInput)); QCOMPARE(window.inputText_->toPlainText(),QString("draft two"));
    QVERIFY(window.coordinator_->submitUserInput(childRequest,"child answer",0)); QTRY_VERIFY(!window.busy()); QVERIFY(window.inputPrompts_.empty()); QVERIFY(!window.inputPanel_->isVisible());
    QVERIFY(window.projects_[0].results.at(2).output == "parent answer"); QVERIFY(window.projects_[1].results.at(2).output == "child answer"); QCOMPARE(window.projects_[1].history.size(),size_t(3));
}
void GuiTest::scopedInputSelectionAndStop() {
    QTemporaryDir directory; Window window(true); window.show(); auto flow=Flow::initial();
    for (Id id : {3u,4u}) { Node node; node.id=id; node.kind=Kind::UserInput; node.title=QString("Question %1").arg(id); node.position={double(id*700),0}; flow.nodes[id]=node; }
    flow.edges={{1,3},{1,4},{3,2},{4,2}}; window.editGraph_->replace(flow); window.changed();
    const auto path=window.documents_[0].path;
    for (QString name : {"First","Second"}) { const auto folder=directory.filePath(name); QVERIFY(QDir().mkpath(folder)); window.projects_.push_back({name,folder,path,{}}); }
    window.refreshProjects(); window.tabs_->setCurrentIndex(1); window.executeFlow(); QTRY_COMPARE(window.inputSelector_->count(),2);
    QCOMPARE(window.inputSelector_->itemText(0),QString("Question 3")); QCOMPARE(window.inputSelector_->itemText(1),QString("Question 4"));
    window.inputText_->setPlainText("first draft"); window.inputSelector_->setCurrentIndex(1); window.inputText_->setPlainText("second draft");
    QTRY_COMPARE(window.workSelected_,Id(4));
    auto *item=window.workScene_->nodeGraphicsObject(4); QVERIFY(item);
    QTRY_VERIFY(window.workView_->mapToScene(window.workView_->viewport()->rect()).boundingRect().contains(item->sceneBoundingRect()));
    const auto center=window.workView_->mapToScene(window.workView_->viewport()->rect().center()); QVERIFY(QLineF(center,item->sceneBoundingRect().center()).length()<5);
    window.inputSelector_->setCurrentIndex(0); QCOMPARE(window.inputText_->toPlainText(),QString("first draft")); window.inputSelector_->setCurrentIndex(1);
    auto *split=window.workInputSplit_; QCoreApplication::processEvents(); const int before=split->sizes()[1]; auto *handle=split->handle(1);
    QTest::mousePress(handle,Qt::LeftButton); QTest::mouseMove(handle,QPoint(handle->width()/2,-45),20); QTest::mouseRelease(handle,Qt::LeftButton,Qt::NoModifier,QPoint(handle->width()/2,-45));
    QTRY_VERIFY(split->sizes()[1]!=before); const int height=split->sizes()[1];
    window.selectProject(1); QVERIFY(window.inputPanel_->isHidden()); QCOMPARE(window.inputSelector_->count(),0); QVERIFY(!window.stopButton_->isEnabled());
    window.selectProject(0); QCOMPARE(window.inputSelector_->currentIndex(),1); QCOMPARE(window.inputText_->toPlainText(),QString("second draft")); QTRY_VERIFY(qAbs(split->sizes()[1]-height)<5);
    window.selectProject(1); window.executeFlow(); QTRY_COMPARE(window.inputPrompts_.size(),size_t(4)); QCOMPARE(window.inputSelector_->count(),2);
    auto stop=[&](QMessageBox::StandardButton answer) { QTimer::singleShot(0,&window,[answer]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); QVERIFY(box); QCOMPARE(box->windowTitle(),QString("フローを停止")); box->button(answer)->click();}); QTest::mouseClick(window.stopButton_,Qt::LeftButton); };
    stop(QMessageBox::Cancel); QVERIFY(window.coordinator_->running(window.projects_[1].historyId)); QCOMPARE(window.inputPrompts_.size(),size_t(4));
    stop(QMessageBox::Ok); QTRY_VERIFY(!window.coordinator_->running(window.projects_[1].historyId)); QVERIFY(window.coordinator_->running(window.projects_[0].historyId));
    QVERIFY(window.inputPanel_->isHidden()); QCOMPARE(window.inputSelector_->count(),0); QCOMPARE(window.inputPrompts_.size(),size_t(2));
    window.selectProject(0); QCOMPARE(window.inputSelector_->currentIndex(),1); QCOMPARE(window.inputText_->toPlainText(),QString("second draft"));
    window.project_=-1; window.refreshUserInputPanel(); QVERIFY(window.inputPanel_->isHidden()); QCOMPARE(window.inputSelector_->count(),0);
    window.selectProject(0); stop(QMessageBox::Ok); QTRY_VERIFY(!window.busy()); QVERIFY(window.inputPanel_->isHidden());
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
    window.editGraph_->replace(Flow::initial()); window.changed();
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
