#include "progress_test.h"
#include "progressview.h"
#include <QtTest>
#include <QDir>
#include <QLabel>
#include <QLayout>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeWidget>

namespace an {
namespace {
QString fixture(const QString &agent) {
    if (agent == "Codex") return R"({"type":"item.started","item":{"id":"c1","type":"command_execution","command":"echo secret","status":"in_progress","aggregated_output":""}}
{"type":"item.updated","item":{"id":"c1","type":"command_execution","command":"echo secret","status":"in_progress","aggregated_output":"o"}}
{"type":"item.completed","item":{"id":"c1","type":"command_execution","command":"echo secret","status":"completed","aggregated_output":"ok","exit_code":0}}
{"type":"item.completed","item":{"id":"f1","type":"file_change","status":"completed","changes":[{"path":"note.txt","kind":"add"}]}}
{"type":"item.completed","item":{"id":"m1","type":"agent_message","text":"**Finished**"}}
)";
    if (agent == "Claude Code") return R"({"type":"assistant","message":{"id":"m1","content":[{"type":"tool_use","id":"t1","name":"Read","input":{"file_path":"note.txt"}}]}}
{"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"t1","content":"ok","is_error":false}]}}
{"type":"assistant","message":{"id":"m2","content":[{"type":"text","text":"**Finished**"}]}}
{"type":"result","is_error":false,"result":"**Finished**"}
)";
    return R"({"type":"tool_use","part":{"id":"t1","tool":"read","state":{"status":"running","input":{"filePath":"note.txt"}}}}
{"type":"tool_use","part":{"id":"t1","tool":"read","state":{"status":"completed","input":{"filePath":"note.txt"},"content":[{"type":"text","text":"ok"}]}}}
{"type":"text","part":{"id":"m1","type":"text","text":"**Finished**"}}
)";
}
QString jsonLine(const Json &value) { return QString::fromStdString(value.dump()) + '\n'; }
QList<const ProgressItem *> ofKind(const ProgressParser &parser, ProgressKind kind) {
    QList<const ProgressItem *> result; for (const auto &entry : parser.items()) if (entry.kind == kind) result.append(&entry); return result;
}
QTreeWidgetItem *field(QTreeWidgetItem *parent, const QString &name) {
    for (int i = 0; i < parent->childCount(); ++i) if (parent->child(i)->text(0).startsWith(name + ": ")) return parent->child(i);
    return nullptr;
}
}
void ProgressTest::protocols_data() {
    QTest::addColumn<QString>("agent");
    for (QString name : {"Codex", "Claude Code", "OpenCode"}) QTest::newRow(qPrintable(name)) << name;
}
void ProgressTest::compactInputMessage() {
    ContentView view; view.setObjectName("userInputMessage"); view.setCompactMessage(3); view.resize(360,100); view.show(); view.setPlainText("Short message");
    auto *browser=view.findChild<QTextBrowser *>(); auto *tree=view.findChild<QTreeWidget *>(); QVERIFY(browser); QVERIFY(tree);
    QTRY_VERIFY(browser->height()<browser->fontMetrics().lineSpacing()*2); QVERIFY(!browser->verticalScrollBar()->isVisible()); QCOMPARE(browser->frameShape(),QFrame::NoFrame);
    QCOMPARE(browser->document()->documentMargin(),qreal(0)); QVERIFY(browser->styleSheet().isEmpty()); QVERIFY(view.styleSheet().contains("background:transparent"));
    view.setPlainText("one\n\ntwo\n\nthree\n\nfour\n\nfive");
    QTRY_VERIFY(browser->verticalScrollBar()->isVisible()); QVERIFY(browser->height()<=int(std::ceil(browser->fontMetrics().lineSpacing()*1.15))*3);
    view.setPlainText(QString("wrapped words ").repeated(100)); QTRY_VERIFY(browser->verticalScrollBar()->maximum()>0);
    view.setPlainText("Short again"); QTRY_VERIFY(!browser->verticalScrollBar()->isVisible()); QTRY_VERIFY(browser->height()<browser->fontMetrics().lineSpacing()*2);
    Json value=Json::object(); for(int i=0;i<12;++i) value["field"+std::to_string(i)]=i; view.setValue(value,true,true);
    QTRY_VERIFY(tree->isVisible()); QTRY_VERIFY(tree->verticalScrollBar()->maximum()>0); QCOMPARE(tree->frameShape(),QFrame::NoFrame); QVERIFY(tree->height()<100);
    ContentView normal; normal.resize(360,100); normal.show(); normal.setPlainText(QString("line\n\n").repeated(20));
    QTRY_VERIFY(normal.findChild<QTextBrowser *>()->height()>200);
}
void ProgressTest::protocols() {
    QFETCH(QString, agent); ProgressParser parser; parser.append(fixture(agent)); parser.finish(State::Succeeded); parser.finalResponse("**Finished**\n");
    const auto tools = ofKind(parser, ProgressKind::Tool); QCOMPARE(tools.size(), 1);
    QCOMPARE(tools[0]->status, QString("succeeded")); QVERIFY(tools[0]->hasInput); QVERIFY(tools[0]->hasOutput);
    QCOMPARE(ofKind(parser, ProgressKind::FileAccess).size(), 1);
    const auto files = ofKind(parser, ProgressKind::FileAccess); QCOMPARE(files[0]->path, QString("note.txt")); QCOMPARE(files[0]->status, QString("succeeded"));
    QCOMPARE(files[0]->name, agent == "Codex" ? QString("作成") : QString("読取"));
    const auto messages = ofKind(parser, ProgressKind::Message); QCOMPARE(messages.size(), 1); QCOMPARE(messages[0]->name, QString("最終応答"));
    QVERIFY(messages[0]->finalResponse);
    ProgressView view; const Json output = "**Finished**\n";
    view.setProgress(fixture(agent), "finished", State::Succeeded, &output);
    for (auto *heading : view.findChildren<QLabel *>("progressHeading")) {
        QVERIFY(!heading->text().contains("最終応答")); QVERIFY(!heading->text().contains("エージェント"));
    }
}
void ProgressTest::claudeStreaming() {
    ProgressParser parser;
    parser.append(R"({"type":"stream_event","event":{"type":"message_start","message":{"id":"m1"}}}
{"type":"stream_event","event":{"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}}
{"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hel"}}}
)" );
    QCOMPARE(ofKind(parser, ProgressKind::Message)[0]->content, Json("Hel"));
    parser.append(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"lo"}}}
{"type":"assistant","message":{"id":"m1","content":[{"type":"text","text":"Hello"}]}}
{"type":"stream_event","event":{"type":"content_block_stop","index":0}}
{"type":"stream_event","event":{"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"t1","name":"Read","input":{}}}}
{"type":"stream_event","event":{"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"{\"file_"}}}
{"type":"stream_event","event":{"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"path\":\"note.txt\"}"}}}
{"type":"assistant","message":{"id":"m1","content":[{"type":"tool_use","id":"t1","name":"Read","input":{"file_path":"note.txt"}}]}}
{"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"t1","is_error":true,"content":{"error":"denied"}}]}}
)" );
    QCOMPARE(ofKind(parser, ProgressKind::Message).size(), 1); QCOMPARE(ofKind(parser, ProgressKind::Message)[0]->content, Json("Hello"));
    QCOMPARE(ofKind(parser, ProgressKind::Tool).size(), 1); QCOMPARE(ofKind(parser, ProgressKind::Tool)[0]->status, QString("failed"));
    QCOMPARE(ofKind(parser, ProgressKind::FileAccess).size(), 1); QCOMPARE(ofKind(parser, ProgressKind::FileAccess)[0]->status, QString("failed"));
    parser.append("{\"type\":\"result\",\"structured_output\":{\"ok\":true}}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Message).last()->content, Json({{"ok", true}}));
    parser.append(R"({"type":"assistant","message":{"id":"other","content":[{"type":"tool_use","id":"parallel1","name":"Write","input":{"file_path":"a.txt"}},{"type":"tool_use","id":"parallel2","name":"Read","input":{"file_path":"b.txt"}}]}}
{"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"parallel2","content":"second","is_error":true},{"type":"tool_result","tool_use_id":"parallel1","content":"first"}]}}
)" );
    const auto tools = ofKind(parser, ProgressKind::Tool); QCOMPARE(tools.size(), 3);
    QCOMPARE(tools[1]->name, QString("Write")); QCOMPARE(tools[1]->status, QString("succeeded")); QCOMPARE(tools[1]->output, Json("first"));
    QCOMPARE(tools[2]->name, QString("Read")); QCOMPARE(tools[2]->status, QString("failed")); QCOMPARE(tools[2]->output, Json("second"));
}
void ProgressTest::openCodePatchAndErrors() {
    ProgressParser parser;
    Json part = {{"id", "p1"}, {"tool", "patch"}};
    part["state"] = {{"status", "completed"}, {"output", "Success"}};
    part["state"]["input"] = {{"patchText", "*** Begin Patch\n*** Add File: a.txt\n+data\n*** Update File: b.txt\n-old\n+new\n*** Delete File: c.txt\n*** End Patch"}};
    part["state"]["metadata"]["metadata"]["files"] = Json::array({{{"file", "a.txt"}, {"status", "added"}}, {{"file", "b.txt"}, {"status", "modified"}}, {{"file", "c.txt"}, {"status", "deleted"}}});
    parser.append(jsonLine({{"type", "tool_use"}, {"part", part}}));
    const auto files = ofKind(parser, ProgressKind::FileAccess); QCOMPARE(files.size(), 3);
    QCOMPARE(files[0]->name, QString("作成")); QCOMPARE(files[1]->name, QString("変更")); QCOMPARE(files[2]->name, QString("削除"));
    part["id"] = "p2"; part["tool"] = "write"; part["state"] = {{"status", "error"}, {"input", {{"filePath", "denied.txt"}}}, {"error", {{"message", "denied"}}}};
    parser.append(jsonLine({{"type", "tool_use"}, {"part", part}}));
    QCOMPARE(ofKind(parser, ProgressKind::Tool).last()->status, QString("failed")); QCOMPARE(ofKind(parser, ProgressKind::FileAccess).last()->status, QString("failed"));
    parser.append(R"({"type":"item.completed","item":{"id":"cmd","type":"command_execution","status":"completed","command":"exit 1","exit_code":1,"aggregated_output":"failed"}}
{"type":"item.completed","item":{"id":"mcp","type":"mcp_tool_call","server":"local","tool":"read","status":"completed","arguments":{"path":"read.txt"},"result":{"content":[{"type":"text","text":"ok"}]}}}
)" );
    QCOMPARE(ofKind(parser, ProgressKind::Tool)[2]->status, QString("failed")); QCOMPARE(ofKind(parser, ProgressKind::Tool).last()->name, QString("local/read"));
    parser.append("{\"type\":\"item.completed\",\"item\":{\"id\":\"search\",\"type\":\"web_search\",\"query\":\"Qt documentation\"}}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Tool).last()->name, QString("web_search")); QVERIFY(!ofKind(parser, ProgressKind::Tool).last()->hasOutput);
}
void ProgressTest::jsonLimitsAndMarkdown() {
    ContentView view; Json value = {{"long", std::string(600, 'a')}, {"array", Json::array()}};
    for (int i = 0; i < 12; ++i) value["array"].push_back(i);
    value["emoji"] = QString::fromUtf8("😀").repeated(550).toStdString(); view.setValue(value); view.show();
    auto *tree = view.findChild<QTreeWidget *>("jsonContent"); QVERIFY(tree && tree->isVisible());
    auto *root = tree->invisibleRootItem(); QCOMPARE(tree->topLevelItemCount(), 3);
    auto *longValue = field(root, "long"); QVERIFY(longValue); QCOMPARE(longValue->text(0).count('a'), 500); QVERIFY(longValue->text(0).contains("...以下 100文字"));
    auto *array = field(root, "array"); QVERIFY(array); QCOMPARE(array->childCount(), 9); QCOMPARE(array->child(8)->text(0), QString("...他 4件の項目"));
    auto *emoji = field(root, "emoji"); QVERIFY(emoji); QCOMPARE(emoji->text(0).count(QString::fromUtf8("😀")), 500); QVERIFY(emoji->text(0).contains("...以下 50文字"));
    QVERIFY(!emoji->text(0).contains(QChar::ReplacementCharacter));
    view.setValue(std::string(600, 'b'), true, true);
    QVERIFY(tree->topLevelItem(0)->text(0).contains("...以下 100文字")); QCOMPARE(tree->topLevelItem(0)->text(0).count('b'), 500);
    view.setPlainText("# Heading\n\n**bold**\n\n```cpp\nint n = 1;\n```\n\n![image](file:///not-allowed.png)");
    auto *browser = view.findChild<QTextBrowser *>("markdownContent"); QVERIFY(browser && browser->isVisible()); QVERIFY(!tree->isVisible());
    const auto cursor = browser->document()->find("bold"); QVERIFY(!cursor.isNull()); QVERIFY(cursor.charFormat().fontWeight() >= QFont::Bold);
    QVERIFY(!browser->openLinks()); QVERIFY(!browser->openExternalLinks());
    QVERIFY(!browser->document()->resource(QTextDocument::ImageResource, QUrl("file:///not-allowed.png")).isValid());
}
void ProgressTest::toolFoldsAndReplay() {
    ProgressView view; view.resize(520, 700); view.show();
    const auto all = fixture("OpenCode"); const auto start = all.left(all.indexOf('\n') + 1);
    view.setProgress(start, "run1", State::Running);
    auto *input = view.findChild<QToolButton *>("toolInputToggle"), *output = view.findChild<QToolButton *>("toolOutputToggle"); QVERIFY(input && output);
    QVERIFY(!input->isChecked() && !output->isChecked()); QCOMPARE(view.findChildren<ContentView *>().size(), 0);
    for (auto *heading : view.findChildren<QLabel *>("progressHeading")) QVERIFY(!heading->text().contains("echo secret"));
    QTest::mouseClick(input, Qt::LeftButton); QVERIFY(input->isChecked()); QVERIFY(!output->isChecked());
    auto *content = view.findChild<ContentView *>(); QVERIFY(content); QVERIFY(content->toPlainText().contains("note.txt"));
    view.setProgress(all, "run1", State::Succeeded); QCOMPARE(view.findChild<QToolButton *>("toolInputToggle"), input); QVERIFY(input->isChecked()); QVERIFY(!output->isChecked());
    QTest::mouseClick(output, Qt::LeftButton); QVERIFY(output->isChecked());
    QCoreApplication::processEvents(); QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-progress-log.png")));
    auto longer = all;
    for (int i = 0; i < 40; ++i) longer += jsonLine({{"type", "unknown_event"}, {"message", "future event"}});
    view.setProgress(longer, "run1", State::Running); QCoreApplication::processEvents(); QCoreApplication::processEvents();
    QTRY_COMPARE(view.verticalScrollBar()->value(), view.verticalScrollBar()->maximum());
    view.verticalScrollBar()->setValue(50); const auto position = view.verticalScrollBar()->value();
    view.setProgress(longer + jsonLine({{"type", "unknown_event"}}), "run1", State::Running); QCoreApplication::processEvents();
    QCOMPARE(view.verticalScrollBar()->value(), position); QVERIFY(input->isChecked());
    view.setProgress(all, "past-run", State::Succeeded); QVERIFY(!view.findChild<QToolButton *>("toolInputToggle")->isChecked());
    QCOMPARE(view.toPlainText(), all);
    const Json finalString = "plain string";
    view.setProgress("{\"type\":\"result\",\"structured_output\":\"plain string\"}\n", "structured-history", State::Succeeded, &finalString);
    QVERIFY(view.finalIsJson());
    QCOMPARE(view.findChildren<QLabel *>("progressHeading").size(), 0);
    const QString streaming = "{\"type\":\"item.completed\",\"item\":{\"id\":\"final\",\"type\":\"agent_message\",\"text\":\"Final answer\"}}\n";
    view.setProgress(streaming, "stream-final", State::Running);
    QCOMPARE(view.findChildren<QLabel *>("progressHeading").size(), 0);
    QCOMPARE(view.findChildren<ContentView *>().size(), 1);
    const Json finalAnswer = "Final answer";
    view.setProgress(streaming, "stream-final", State::Succeeded, &finalAnswer);
    QCOMPARE(view.findChildren<QLabel *>("progressHeading").size(), 0);
    view.setProgress(streaming, "replay-final", State::Succeeded, &finalAnswer);
    QCOMPARE(view.findChildren<QLabel *>("progressHeading").size(), 0);
    ProgressParser relative; relative.setDirectory("C:/project"); relative.append(fixture("Claude Code"));
    QCOMPARE(ofKind(relative, ProgressKind::FileAccess)[0]->path, QString("C:/project/note.txt"));
}
void ProgressTest::borderlessLayout() {
    ProgressView view; view.resize(640, 700); view.show();
    const auto log = fixture("OpenCode") + jsonLine({{"type", "future_event"}, {"message", "説明"}}) + "plain stderr\n";
    view.setProgress(log, "layout", State::Running); QCoreApplication::processEvents();
    for (auto *heading : view.findChildren<QLabel *>("progressHeading")) {
        QVERIFY(heading->font().bold()); QVERIFY(!heading->text().contains("エージェント"));
    }
    auto *detail = view.findChild<QToolButton *>("eventDetailToggle"); QVERIFY(detail);
    // The plain stderr row has no details; the structured fallback shares a header row.
    QLabel *fallback = nullptr;
    for (auto *heading : view.findChildren<QLabel *>("progressHeading")) if (heading->text().startsWith("future_event")) fallback = heading;
    QVERIFY(fallback); QCOMPARE(fallback->parentWidget(), detail->parentWidget());
    QCOMPARE(fallback->geometry().center().y(), detail->geometry().center().y());
    QCOMPARE(detail->text(), QString("詳細")); QCOMPARE(detail->arrowType(), Qt::RightArrow); QVERIFY(detail->autoRaise()); QVERIFY(!detail->isChecked());
    auto *input = view.findChild<QToolButton *>("toolInputToggle"); QVERIFY(input && input->autoRaise());
    QVERIFY(input->sizeHint().width() >= input->fontMetrics().horizontalAdvance(input->text()) + 24);
    auto *toolHeading = input->parentWidget()->parentWidget()->findChild<QLabel *>("progressHeading"); QVERIFY(toolHeading);
    QVERIFY(input->mapTo(&view, QPoint()).x() >= toolHeading->mapTo(&view, QPoint()).x() + 12);
    QTest::mouseClick(detail, Qt::LeftButton); QVERIFY(detail->isChecked()); QCOMPARE(detail->arrowType(), Qt::DownArrow);
    auto *content = detail->parentWidget()->parentWidget()->findChild<ContentView *>(); QVERIFY(content && content->isVisible());
    view.setProgress(log + jsonLine({{"type", "another_event"}}), "layout", State::Running); QVERIFY(detail->isChecked());
    QTest::mouseClick(detail, Qt::LeftButton); QVERIFY(!content->isVisible());
    QCoreApplication::processEvents(); QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-progress-borderless.png")));
}
void ProgressTest::compactJsonTree() {
    ContentView view; view.resize(600, 300); view.setValue(Json{{"one", 1}, {"two", 2}}); view.show();
    QCoreApplication::processEvents(); auto *tree = view.findChild<QTreeWidget *>("jsonContent"); QVERIFY(tree);
    QCOMPARE(tree->topLevelItemCount(), 2); QCOMPARE(tree->topLevelItem(0)->text(0), QString("one: 1"));
    const int bottom = tree->visualItemRect(tree->topLevelItem(1)).bottom() + 1;
    QVERIFY(tree->viewport()->height() - bottom <= 2);
    view.setValue(Json::array({1, 2, 3, 4, 5, 6, 7, 8, 9})); QCoreApplication::processEvents();
    QCOMPARE(tree->topLevelItemCount(), 9); QCOMPARE(tree->topLevelItem(0)->text(0), QString("[0]: 1"));
    QCOMPARE(tree->topLevelItem(8)->text(0), QString("...他 1件の項目"));
    view.setValue(Json{{"nested", {{"child", 1}}}}); QCoreApplication::processEvents();
    auto *nested = tree->topLevelItem(0); nested->setExpanded(true); QCoreApplication::processEvents();
    const int expanded = tree->height(); nested->setExpanded(false); QCoreApplication::processEvents(); QVERIFY(tree->height() < expanded);
    view.setValue(Json{{"nested", {{"child", 2}}}}); QVERIFY(!tree->topLevelItem(0)->isExpanded());
    view.setValue(42); QCOMPARE(tree->topLevelItem(0)->text(0), QString("42"));
    view.setValue(Json::object()); QCOMPARE(tree->topLevelItem(0)->text(0), QString("{0項目}"));
}
void ProgressTest::roundedCodeBlocks() {
    ContentView view; view.resize(560, 300);
    view.setPlainText("Paragraph\n\n```cpp\nint n = 1;\nreturn n;\n```\n\nAfter code"); view.show();
    QCoreApplication::processEvents();
    auto *browser = view.findChild<QTextBrowser *>("markdownContent"); QVERIFY(browser);
    const auto code = browser->document()->find("int n = 1;"); QVERIFY(!code.isNull());
    QCOMPARE(code.blockFormat().lineHeight(), qreal(115));
    QCOMPARE(browser->document()->find("Paragraph").blockFormat().lineHeight(), qreal(115));
    QCOMPARE(code.blockFormat().leftMargin(), qreal(6)); QCOMPARE(code.blockFormat().topMargin(), qreal(14));
    const auto image = browser->viewport()->grab().toImage();
    int backgroundPixels = 0; QRect backgroundBounds;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x)
        if (image.pixelColor(x, y) == QColor("#273449")) { ++backgroundPixels; backgroundBounds = backgroundBounds.united(QRect(x, y, 1, 1)); }
    QVERIFY(backgroundPixels > 100);
    QVERIFY(image.pixelColor(backgroundBounds.topLeft()) != QColor("#273449")); // Rounded, not a filled rectangular corner.
    QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-markdown-code.png")));
    browser->resize(280, browser->height()); QCoreApplication::processEvents();
    QCOMPARE(browser->document()->find("return n;").selectedText(), QString("return n;"));
}
void ProgressTest::inlineCodeBackground() {
    ContentView view; view.resize(560, 160);
    view.setPlainText("Normal text with `inline_code` and **bold** and `second_code`.\n\n```cpp\nint n = 1;\n```"); view.show();
    QCoreApplication::processEvents();
    auto *browser = view.findChild<QTextBrowser *>("markdownContent"); QVERIFY(browser);
    const auto image = browser->viewport()->grab().toImage();
    for (const auto &text : {QString("inline_code"), QString("second_code")}) {
        const auto code = browser->document()->find(text); QVERIFY(!code.isNull());
        auto start = code; start.setPosition(code.selectionStart());
        const auto rect = browser->cursorRect(start);
        const auto padded = QPoint(rect.left() - 2, rect.center().y());
        QCOMPARE(image.pixelColor(padded * image.devicePixelRatio()), QColor("#273449"));
        QVERIFY(image.pixelColor(QPoint(rect.left() - 3, rect.top() - 2) * image.devicePixelRatio()) != QColor("#273449"));
        QCOMPARE(code.selectedText(), text);
    }
    QVERIFY(browser->document()->find("Normal text").charFormat().background().style() == Qt::NoBrush);
    QVERIFY(browser->document()->find("bold").charFormat().background().style() == Qt::NoBrush);
    QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-markdown-inline-code.png")));
    view.setValue(Json("Plain `inline_code`"), false);
    QVERIFY(browser->document()->find("inline_code").charFormat().background().style() == Qt::NoBrush);
    QVERIFY(browser->document()->begin().blockFormat().lineHeight() != 115);
}
void ProgressTest::disclosureHover() {
    ProgressView view; view.resize(640, 700); view.show();
    view.setProgress(fixture("OpenCode") + jsonLine({{"type", "future_event"}}), "hover", State::Running);
    QCoreApplication::processEvents();
    for (const auto &name : {"toolInputToggle", "toolOutputToggle", "eventDetailToggle"}) {
        auto *button = view.findChild<QToolButton *>(name); QVERIFY(button);
        QTest::mouseMove(view.viewport(), QPoint(600, 650)); QCoreApplication::processEvents();
        const auto normal = button->grab().toImage();
        QTest::mouseMove(button, button->rect().center()); QCoreApplication::processEvents();
        auto image = button->grab().toImage();
        const QPoint sample(1, button->height() / 2);
        QCOMPARE(image.pixelColor(sample * image.devicePixelRatio()), normal.pixelColor(sample * normal.devicePixelRatio()));
        QVERIFY(image != normal);
        QVERIFY(!button->isChecked());
        QTest::mouseMove(view.viewport(), QPoint(600, 650)); QCoreApplication::processEvents();
        image = button->grab().toImage();
        QCOMPARE(image, normal);
    }
}
void ProgressTest::longMarkdownMessage() {
    ProgressView view; view.resize(520, 400); view.show();
    QString text; for (int i = 0; i < 100; ++i) text += QString("Line %1 with **Markdown** and some wrapping text.\n\n").arg(i);
    auto message = [&](const QString &content) { return jsonLine({{"type", "item.completed"}, {"item", {{"id", "long"}, {"type", "agent_message"}, {"text", content.toStdString()}}}}); };
    const auto log = message(text); view.setProgress(log, "long", State::Running);
    auto *browser = view.findChild<QTextBrowser *>("markdownContent"); QVERIFY(browser);
    QTRY_VERIFY(browser->height() > 420); QTRY_VERIFY(view.verticalScrollBar()->maximum() > 0);
    QCOMPARE(browser->verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    QTRY_COMPARE(browser->verticalScrollBar()->maximum(), 0);
    view.resize(320, 400); QCoreApplication::processEvents();
    QTRY_COMPARE(browser->verticalScrollBar()->maximum(), 0);
    view.setProgress(log + message(text + "Last line\n"), "long", State::Running);
    QTRY_VERIFY(browser->toPlainText().trimmed().endsWith("Last line"));
    QTRY_COMPARE(browser->verticalScrollBar()->maximum(), 0);
}
void ProgressTest::claudeThinking() {
    const QString start = R"({"type":"stream_event","event":{"type":"message_start","message":{"id":"m-think"}}}
{"type":"stream_event","event":{"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":"","signature":""}}}
)";
    const QString rest = R"({"type":"system","subtype":"thinking_tokens","estimated_tokens":50,"estimated_tokens_delta":50}
{"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"private thought","estimated_tokens":50}}}
{"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"signature_delta","signature":"private signature"}}}
{"type":"assistant","message":{"id":"m-think","content":[{"type":"thinking","thinking":"private thought","signature":"private signature"}]}}
{"type":"stream_event","event":{"type":"content_block_stop","index":0}}
{"type":"stream_event","event":{"type":"message_delta","usage":{"output_tokens_details":{"thinking_tokens":84}}}}
{"type":"stream_event","event":{"type":"message_stop"}}
)";
    ProgressParser parser; parser.append(start); QCOMPARE(parser.items().size(), size_t(1)); parser.append(rest);
    QCOMPARE(parser.items().size(), size_t(1)); QCOMPARE(parser.items()[0].kind, ProgressKind::Thinking);
    QCOMPARE(parser.items()[0].name, QString("思考中")); QVERIFY(parser.items()[0].content.is_null());
    ProgressView view; view.show(); view.setProgress(start, "thinking", State::Running);
    auto *heading = view.findChild<QLabel *>("progressHeading"); QVERIFY(heading && heading->font().bold()); QCOMPARE(heading->text(), QString("思考中"));
    view.setProgress(start + rest, "thinking", State::Succeeded); QCOMPARE(view.findChild<QLabel *>("progressHeading"), heading);
    QVERIFY(view.findChildren<QToolButton *>().isEmpty()); QCOMPARE(view.toPlainText(), start + rest);
    ProgressParser replay; replay.append("{\"type\":\"assistant\",\"message\":{\"id\":\"complete\",\"content\":[{\"type\":\"thinking\",\"thinking\":\"private\"}]}}\n");
    QCOMPARE(ofKind(replay, ProgressKind::Thinking).size(), 1);
    // Thinking deltas without a start (truncated logs) remain hidden.
    ProgressParser truncated; truncated.append(rest.left(rest.indexOf("{\"type\":\"assistant\""))); QVERIFY(truncated.items().empty());
}
void ProgressTest::executionBoundaries() {
    for (const auto &types : {QStringList{"step_start", "step_finish"}, QStringList{"turn.started", "turn.completed"}, QStringList{"turn.start", "turn.completed"}}) {
        auto boundary = [&](int i) { return jsonLine({{"type", types[i].toStdString()}}); };
        auto message = [](const char *id) { return jsonLine({{"type", "item.completed"}, {"item", {{"id", id}, {"type", "agent_message"}, {"text", "Message"}}}}); };
        const auto first = message("1"), second = message("2");
        ProgressParser parser; parser.append(boundary(0) + first); QCOMPARE(parser.items().size(), size_t(1)); QVERIFY(!parser.items()[0].spaceBefore);
        parser.append(boundary(1) + boundary(0)); QCOMPARE(parser.items().size(), size_t(1));
        parser.append(first + second); QCOMPARE(parser.items().size(), size_t(2)); QVERIFY(parser.items()[1].spaceBefore);
        ProgressView view; view.show(); view.setProgress(boundary(0) + first + boundary(1) + boundary(0) + second, types[0], State::Running);
        const auto contents = view.findChildren<ContentView *>(); QCOMPARE(contents.size(), 2);
        QCOMPARE(contents[0]->parentWidget()->layout()->contentsMargins().top(), 2);
        QCOMPARE(contents[1]->parentWidget()->layout()->contentsMargins().top(), 12);
        QVERIFY(view.findChildren<QToolButton *>().isEmpty());
    }
}
void ProgressTest::commandPresentation() {
    const QString powershell = R"({"type":"stream_event","event":{"type":"message_start","message":{"id":"powershell"}}}
{"type":"stream_event","event":{"type":"content_block_start","index":0,"content_block":{"type":"tool_use","id":"ps1","name":"PowerShell","input":{}}}}
{"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"input_json_delta","partial_json":"{\"command\":\"Get-Location\"}"}}}
{"type":"assistant","message":{"id":"powershell","content":[{"type":"tool_use","id":"ps1","name":"PowerShell","input":{"command":"Get-Location","description":"Current directory"}}]}}
{"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"ps1","content":"C:/project","is_error":false}]}}
)";
    for (const auto &agent : {QString("Codex"), QString("Claude Code")}) {
        const auto log = agent == "Codex" ? fixture(agent) : powershell;
        ProgressParser parser; parser.append(log); const auto tools = ofKind(parser, ProgressKind::Tool); QCOMPARE(tools.size(), 1);
        const auto label = agent == "Codex" ? QString("シェル実行") : QString("PowerShell");
        QCOMPARE(tools[0]->commandLabel, label); QVERIFY(!tools[0]->command.isEmpty());
        ProgressView view; view.resize(640, 500); view.show(); view.setProgress(log, agent, State::Succeeded); QCoreApplication::processEvents();
        auto *input = view.findChild<QToolButton *>("toolInputToggle"); QVERIFY(input && !input->isVisible());
        auto *output = view.findChild<QToolButton *>("toolOutputToggle"); QVERIFY(output && output->isVisible() && !output->isChecked());
        auto *heading = view.findChild<QLabel *>("progressHeading"); QVERIFY(heading && heading->font().bold());
        QCOMPARE(heading->text(), label);
        auto *preview = view.findChild<QLabel *>("commandPreview"); QVERIFY(preview); QCOMPARE(preview->text(), tools[0]->command);
        auto *toggle = view.findChild<QToolButton *>("commandToggle"); QVERIFY(toggle && toggle->isVisible() && !toggle->isChecked());
        auto *full = view.findChild<QLabel *>("commandFull"); QVERIFY(full && !full->isVisible());
        QTest::mouseClick(toggle, Qt::LeftButton); QVERIFY(full->isVisible()); QCOMPARE(full->text(), tools[0]->command);
        QTest::mouseClick(output, Qt::LeftButton); QVERIFY(output->isChecked());
        view.setProgress(log, agent, State::Succeeded); QVERIFY(output->isChecked());
        QVERIFY(toggle->isChecked()); QVERIFY(full->isVisible());
        const auto contents = output->parentWidget()->findChildren<ContentView *>(); QCOMPARE(contents.size(), 1); QVERIFY(!contents[0]->toPlainText().isEmpty());
        QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-command-" + agent.section(' ', 0, 0) + ".png")));
    }
}
void ProgressTest::commandElisionAndStatus() {
    const QString command = "Get-ChildItem \"<folder>&\";\n" + QString("Write-Output 'long command'; ").repeated(30);
    auto event = [&](const char *status) { return jsonLine({{"type", "item.completed"}, {"item", {{"id", "c1"}, {"type", "command_execution"}, {"command", command.toStdString()}, {"status", status}, {"aggregated_output", "result"}}}}); };
    ProgressView view; view.resize(420, 500); view.show(); const auto log = event("completed"); view.setProgress(log, "long-command", State::Running);
    QCoreApplication::processEvents();
    auto *preview = view.findChild<QLabel *>("commandPreview"); auto *toggle = view.findChild<QToolButton *>("commandToggle");
    auto *full = view.findChild<QLabel *>("commandFull"); auto *status = view.findChild<QLabel *>("toolStatus");
    QVERIFY(preview && toggle && full && status);
    QVERIFY(!preview->wordWrap()); QVERIFY(!preview->text().contains('\n')); QVERIFY(preview->text().endsWith("..."));
    QVERIFY(preview->styleSheet().contains("#aeb7c4")); QVERIFY(!preview->font().bold());
    QVERIFY(preview->fontMetrics().horizontalAdvance(preview->text()) <= preview->width());
    QCOMPARE(status->text(), QString("完了")); QVERIFY(status->styleSheet().contains("#79d99b"));
    QVERIFY(toggle->mapTo(&view, QPoint()).x() > preview->mapTo(&view, QPoint()).x() + preview->width());
    QTest::mouseClick(toggle, Qt::LeftButton); QCOMPARE(full->text(), command); QVERIFY(full->isVisible()); QCOMPARE(full->textFormat(), Qt::PlainText);
    const auto width = preview->width(); view.resize(300, 500); QCoreApplication::processEvents(); QVERIFY(preview->width() < width);
    QVERIFY(preview->fontMetrics().horizontalAdvance(preview->text()) <= preview->width());
    view.setProgress(log + event("failed"), "long-command", State::Running); QCoreApplication::processEvents();
    QVERIFY(toggle->isChecked()); QCOMPARE(status->text(), QString("失敗")); QVERIFY(status->styleSheet().contains("#ff8585"));
    QTest::mouseClick(toggle, Qt::LeftButton); QVERIFY(!full->isVisible());
    view.setProgress(fixture("Claude Code"), "generic-tool", State::Succeeded); QCoreApplication::processEvents();
    status = view.findChild<QLabel *>("toolStatus"); QVERIFY(status && status->styleSheet().contains("#79d99b"));
    view.setProgress(fixture("Claude Code").replace("\"is_error\":false", "\"is_error\":true"), "generic-error", State::Failed);
    status = view.findChild<QLabel *>("toolStatus"); QVERIFY(status && status->styleSheet().contains("#ff8585"));
    QVERIFY(!view.findChild<QToolButton *>("commandToggle")->isVisible());
}
void ProgressTest::ignoredControlEvents() {
    const QString log = R"({"type":"rate_limit_event","rate_limit_info":{"status":"allowed"}}
{"type":"system","subtype":"init","tools":["Read"]}
{"type":"thread.started","thread_id":"thread1"}
)";
    ProgressParser parser; parser.append(log); QVERIFY(parser.items().empty());
    ProgressView view; view.setProgress(log, "control", State::Succeeded); QVERIFY(view.findChildren<QLabel *>("progressHeading").isEmpty()); QCOMPARE(view.toPlainText(), log);
    parser.append("{\"type\":\"system\",\"subtype\":\"warning\",\"message\":\"keep warning\"}\n");
    QCOMPARE(parser.items().size(), size_t(1)); QCOMPARE(parser.items()[0].text, QString("warning"));
}
void ProgressTest::rowClickTargets() {
    ProgressView view; view.resize(640, 600); view.show();
    const auto log = fixture("Codex") + jsonLine({{"type", "future_event"}, {"subtype", "notice"}});
    view.setProgress(log, "row-clicks", State::Running); QCoreApplication::processEvents();
    auto *row = view.findChild<QWidget *>("commandHeader"); auto *toggle = view.findChild<QToolButton *>("commandToggle"); QVERIFY(row && toggle);
    auto *preview = view.findChild<QLabel *>("commandPreview"); QVERIFY(preview);
    QTest::mouseClick(preview, Qt::LeftButton); QVERIFY(toggle->isChecked());
    QTest::mouseClick(row, Qt::LeftButton, Qt::NoModifier, QPoint(row->width() / 2, row->height() / 2)); QVERIFY(!toggle->isChecked());
    QTest::mouseClick(toggle, Qt::LeftButton); QVERIFY(toggle->isChecked()); // A child-button click must not toggle twice.
    auto *status = view.findChild<QLabel *>("toolStatus"); QTest::mouseClick(status, Qt::LeftButton); QVERIFY(!toggle->isChecked());
    QVERIFY(status->mapTo(row, QPoint()).x() - (preview->mapTo(row, QPoint()).x() + preview->width()) <= 4);
    auto *fallback = view.findChild<QWidget *>("fallbackHeader"); auto *details = view.findChild<QToolButton *>("eventDetailToggle"); QVERIFY(fallback && details);
    auto *heading = fallback->findChild<QLabel *>("progressHeading"); QVERIFY(heading);
    QTest::mouseClick(heading, Qt::LeftButton); QVERIFY(details->isChecked());
    QTest::mouseClick(fallback, Qt::LeftButton, Qt::NoModifier, QPoint(fallback->width() - 4, fallback->height() / 2)); QVERIFY(!details->isChecked());
    QTest::mouseClick(details, Qt::LeftButton); QVERIFY(details->isChecked());
    view.setProgress(log, "row-clicks", State::Running); QVERIFY(details->isChecked());
}
void ProgressTest::rowHoverAndIconPlacement() {
    auto command = [](const char *id, const char *text) {
        return jsonLine({{"type", "item.completed"}, {"item", {{"id", id}, {"type", "command_execution"}, {"command", text}, {"status", "completed"}, {"aggregated_output", "ok"}}}});
    };
    ProgressView view; view.resize(640, 600); view.show();
    view.setProgress(command("short", "echo a") + command("long", "echo a longer command") + jsonLine({{"type", "future_event"}, {"subtype", "notice"}}), "hover-rows", State::Running);
    QCoreApplication::processEvents();
    const auto rows = view.findChildren<QWidget *>("commandHeader"); QCOMPARE(rows.size(), 2);
    for (auto *row : rows) {
        auto *button = row->findChild<QToolButton *>("commandToggle"); auto *status = row->findChild<QLabel *>("toolStatus"); QVERIFY(button && status);
        QCOMPARE(button->x() - (status->x() + status->width()), 4);
    }
    QVERIFY(rows[0]->findChild<QToolButton *>("commandToggle")->x() < rows[1]->findChild<QToolButton *>("commandToggle")->x());
    auto *row = rows[0]; auto *button = row->findChild<QToolButton *>("commandToggle");
    const auto moveAway = [&] { QTest::mouseMove(view.viewport(), QPoint(600, 550)); QCoreApplication::processEvents(); };
    moveAway(); const auto normal = button->grab().toImage();
    for (const auto &name : {"progressHeading", "commandPreview", "toolStatus"}) {
        auto *target = row->findChild<QLabel *>(name); QVERIFY(target);
        QTest::mouseMove(target, target->rect().center()); QCoreApplication::processEvents();
        QVERIFY(button->property("rowHovered").toBool()); QVERIFY(button->grab().toImage() != normal); QVERIFY(!button->isChecked());
        moveAway(); QVERIFY(!button->property("rowHovered").toBool()); QCOMPARE(button->grab().toImage(), normal);
    }
    QTest::mouseMove(row, QPoint(row->width() - 4, row->height() / 2)); QCoreApplication::processEvents();
    QVERIFY(button->property("rowHovered").toBool()); QVERIFY(button->grab().toImage() != normal);
    moveAway();
    auto *fallback = view.findChild<QWidget *>("fallbackHeader"); QVERIFY(fallback);
    auto *detail = fallback->findChild<QToolButton *>("eventDetailToggle"); QVERIFY(detail);
    const auto normalDetail = detail->grab().toImage();
    auto *heading = fallback->findChild<QLabel *>("progressHeading"); QVERIFY(heading);
    QTest::mouseMove(heading, heading->rect().center()); QCoreApplication::processEvents();
    QVERIFY(detail->property("rowHovered").toBool()); QVERIFY(detail->grab().toImage() != normalDetail);
    QTest::mouseMove(fallback, QPoint(fallback->width() - 4, fallback->height() / 2)); QCoreApplication::processEvents();
    QVERIFY(detail->property("rowHovered").toBool()); QVERIFY(detail->grab().toImage() != normalDetail);
    moveAway(); QCOMPARE(detail->grab().toImage(), normalDetail);
}
void ProgressTest::globPresentation() {
    const Json part = {{"id", "glob1"}, {"tool", "glob"}, {"state", {{"status", "completed"}, {"input", {{"pattern", "**/*.{cpp,h}"}, {"limit", 100}}}, {"output", "src/main.cpp"}}}};
    const auto log = jsonLine({{"type", "tool_use"}, {"part", part}});
    ProgressParser parser; parser.append(log); const auto tools = ofKind(parser, ProgressKind::Tool); QCOMPARE(tools.size(), 1);
    QCOMPARE(tools[0]->commandLabel, QString("glob")); QCOMPARE(tools[0]->command, QString("**/*.{cpp,h}")); QVERIFY(ofKind(parser, ProgressKind::FileAccess).isEmpty());
    ProgressView view; view.resize(500, 400); view.show(); view.setProgress(log, "glob", State::Succeeded); QCoreApplication::processEvents();
    QCOMPARE(view.findChild<QLabel *>("progressHeading")->text(), QString("glob"));
    QCOMPARE(view.findChild<QLabel *>("commandPreview")->text(), QString("**/*.{cpp,h}"));
    QVERIFY(!view.findChild<QToolButton *>("toolInputToggle")->isVisible());
    auto *toggle = view.findChild<QToolButton *>("commandToggle"); QVERIFY(toggle);
    QTest::mouseClick(view.findChild<QWidget *>("commandHeader"), Qt::LeftButton); QVERIFY(toggle->isChecked());
    QCOMPARE(view.findChild<QLabel *>("commandFull")->text(), QString("**/*.{cpp,h}"));
    auto *output = view.findChild<QToolButton *>("toolOutputToggle"); QVERIFY(output && !output->isChecked());
    QTest::mouseClick(output, Qt::LeftButton); QVERIFY(view.findChild<ContentView *>()->toPlainText().contains("src/main.cpp"));
    QCoreApplication::processEvents(); QVERIFY(view.grab().save(QDir(AGENTNODIFY_TEST_OUTPUT_DIR).filePath("gui-glob.png")));
}
void ProgressTest::fileToolPresentation() {
    const QString write = R"({"type":"assistant","message":{"id":"write-msg","content":[{"type":"tool_use","id":"write1","name":"Write","input":{"file_path":"note.txt","content":"text"}}]}}
{"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"write1","content":"done","is_error":false}]}}
)";
    Json part = {{"id", "patch1"}, {"tool", "patch"}, {"state", {{"status", "completed"}, {"input", {{"patchText", "*** Begin Patch\n*** Add File: a.txt\n+text\n*** Update File: b.txt\n-old\n+new\n*** End Patch"}}}, {"output", "Success"}}}};
    const auto patch = jsonLine({{"type", "tool_use"}, {"part", part}});
    for (const auto &log : {write, patch}) {
        ProgressParser parser; parser.append(log); QVERIFY(ofKind(parser, ProgressKind::Tool)[0]->hidden); QVERIFY(!ofKind(parser, ProgressKind::FileAccess).isEmpty());
        ProgressView view; view.show(); view.setProgress(log, "file-summary", State::Succeeded); QCoreApplication::processEvents();
        QVERIFY(view.findChildren<QToolButton *>().isEmpty());
        for (auto *status : view.findChildren<QLabel *>("fileStatus")) { QCOMPARE(status->text(), QString("完了")); QVERIFY(status->styleSheet().contains("#79d99b")); }
        const auto failed = log.contains("is_error") ? QString(log).replace("\"is_error\":false", "\"is_error\":true") : QString(log).replace("\"completed\"", "\"error\"");
        view.setProgress(failed, "file-failed", State::Failed);
        for (auto *status : view.findChildren<QLabel *>("fileStatus")) { QCOMPARE(status->text(), QString("失敗")); QVERIFY(status->styleSheet().contains("#ff8585")); }
    }
    // Metadata-only patches and paths unavailable at tool start are handled too.
    part["state"]["input"] = Json::object(); part["state"]["metadata"] = {{"metadata", {{"files", Json::array({{{"file", "a.txt"}, {"status", "added"}}})}}}};
    ProgressParser metadata; metadata.append(jsonLine({{"type", "tool_use"}, {"part", part}}));
    QVERIFY(ofKind(metadata, ProgressKind::Tool)[0]->hidden); QCOMPARE(ofKind(metadata, ProgressKind::FileAccess).size(), 1);
    const auto start = jsonLine({{"type", "tool_use"}, {"part", {{"id", "patch1"}, {"tool", "patch"}, {"state", {{"status", "running"}, {"input", Json::object()}}}}}});
    ProgressView live; live.show(); live.setProgress(start, "partial-patch", State::Running); QVERIFY(live.findChild<QLabel *>("toolStatus"));
    live.setProgress(start + patch, "partial-patch", State::Succeeded);
    QVERIFY(!live.findChild<QLabel *>("toolStatus")); QCOMPARE(live.findChildren<QLabel *>("fileStatus").size(), 2);
    for (auto *status : live.findChildren<QLabel *>("fileStatus")) QVERIFY(status->styleSheet().contains("#79d99b"));
    // A late running update must not regress an already completed file status.
    ProgressParser late; late.append(patch + QString(patch).replace("\"completed\"", "\"running\""));
    for (const auto *file : ofKind(late, ProgressKind::FileAccess)) QCOMPARE(file->status, QString("succeeded"));
    ProgressView read; read.setProgress(fixture("Claude Code"), "read", State::Succeeded);
    QVERIFY(read.findChild<QLabel *>("fileStatus")->styleSheet().contains("#79d99b"));
    ProgressView codex; codex.setProgress(fixture("Codex"), "codex", State::Succeeded);
    QVERIFY(codex.findChild<QLabel *>("fileStatus")->styleSheet().contains("#79d99b"));
}
void ProgressTest::fallbackAndPartialLines() {
    ProgressParser parser; const auto log = fixture("OpenCode"); const auto end = log.indexOf('\n');
    parser.append(log.left(12)); QVERIFY(parser.items().empty()); parser.append(log.mid(12, end + 1 - 12));
    QCOMPARE(ofKind(parser, ProgressKind::Tool).size(), 1); parser.finish(State::Cancelled);
    QCOMPARE(ofKind(parser, ProgressKind::Tool)[0]->status, QString("cancelled"));
    parser.append("{broken json\n"); parser.append("{\"type\":\"future_event\",\"data\":[1,2,3]}\n"); parser.append("stderr without newline"); parser.finish(State::Failed);
    const auto simple = ofKind(parser, ProgressKind::Simple); QCOMPARE(simple.size(), 3); QCOMPARE(simple[1]->name, QString("future_event"));
    QVERIFY(simple[0]->text.contains("broken")); QCOMPARE(simple[2]->text, QString("stderr without newline"));
    parser.append("{\"type\":\"system\",\"subtype\":\"notice\",\"message\":\"long message\"}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Simple).last()->text, QString("notice"));
    parser.append("{\"type\":\"system\",\"subtype\":\"\",\"message\":\"message only\"}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Simple).last()->text, QString("message only"));
}
}
QTEST_MAIN(an::ProgressTest)
