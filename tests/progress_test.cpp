#include "progress_test.h"
#include "progressview.h"
#include <QtTest>
#include <QDir>
#include <QLabel>
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
    const auto all = fixture("Codex"); const auto start = all.left(all.indexOf('\n') + 1);
    view.setProgress(start, "run1", State::Running);
    auto *input = view.findChild<QToolButton *>("toolInputToggle"), *output = view.findChild<QToolButton *>("toolOutputToggle"); QVERIFY(input && output);
    QVERIFY(!input->isChecked() && !output->isChecked()); QCOMPARE(view.findChildren<ContentView *>().size(), 0);
    for (auto *heading : view.findChildren<QLabel *>("progressHeading")) QVERIFY(!heading->text().contains("echo secret"));
    QTest::mouseClick(input, Qt::LeftButton); QVERIFY(input->isChecked()); QVERIFY(!output->isChecked());
    auto *content = view.findChild<ContentView *>(); QVERIFY(content); QVERIFY(content->toPlainText().contains("echo secret"));
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
    const auto log = fixture("Codex") + jsonLine({{"type", "future_event"}, {"message", "説明"}}) + "plain stderr\n";
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
    auto *content = detail->parentWidget()->findChild<ContentView *>(); QVERIFY(content && content->isVisible());
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
    view.setProgress(fixture("Codex") + jsonLine({{"type", "future_event"}}), "hover", State::Running);
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
void ProgressTest::fallbackAndPartialLines() {
    ProgressParser parser; const auto log = fixture("OpenCode"); const auto end = log.indexOf('\n');
    parser.append(log.left(12)); QVERIFY(parser.items().empty()); parser.append(log.mid(12, end + 1 - 12));
    QCOMPARE(ofKind(parser, ProgressKind::Tool).size(), 1); parser.finish(State::Cancelled);
    QCOMPARE(ofKind(parser, ProgressKind::Tool)[0]->status, QString("cancelled"));
    parser.append("{broken json\n"); parser.append("{\"type\":\"future_event\",\"data\":[1,2,3]}\n"); parser.append("stderr without newline"); parser.finish(State::Failed);
    const auto simple = ofKind(parser, ProgressKind::Simple); QCOMPARE(simple.size(), 3); QCOMPARE(simple[1]->name, QString("future_event"));
    QVERIFY(simple[0]->text.contains("broken")); QCOMPARE(simple[2]->text, QString("stderr without newline"));
    parser.append("{\"type\":\"system\",\"subtype\":\"init\",\"message\":\"long message\"}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Simple).last()->text, QString("init"));
    parser.append("{\"type\":\"system\",\"subtype\":\"\",\"message\":\"message only\"}\n");
    QCOMPARE(ofKind(parser, ProgressKind::Simple).last()->text, QString("message only"));
}
}
QTEST_MAIN(an::ProgressTest)
