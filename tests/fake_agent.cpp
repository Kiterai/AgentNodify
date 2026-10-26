// Deterministic CLI protocol fixture, never calls an AI service.
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QRegularExpression>
#include <cstdio>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); auto args = app.arguments();
    QFile input; if (!input.open(stdin, QIODevice::ReadOnly)) return 11; QByteArray prompt;
    if (!args.contains("run")) prompt = input.readAll();
    else prompt = args.last().toUtf8();
    if (prompt.contains("FAIL")) return 7;
    if (prompt.contains("SLEEP")) QThread::sleep(5);
    if (prompt.contains("SHORT_DELAY")) QThread::msleep(80);
    if (prompt.contains("CHECK_CWD") && !QFile::exists("marker.txt")) return 8;
    if (prompt.contains("CHECK_CWD")) {
        if (QDir::cleanPath(qEnvironmentVariable("PWD")).compare(QDir::currentPath(), Qt::CaseInsensitive) != 0) return 18;
        if (args.contains("exec") && args.value(args.indexOf("--cd") + 1) != QDir::currentPath()) return 19;
        if (args.contains("run") && !args.contains("--standalone")) return 20;
    }
    if (prompt.contains("EXPECT_OK") && !prompt.contains("EXPECT_OK=ok")) return 10;
    if (prompt.contains("CHECK_EFFORT_HIGH")) {
        if (args.contains("exec")) {
            if (args.value(args.indexOf("-c") + 1) != "model_reasoning_effort=\"high\"" || args.value(args.indexOf("--sandbox") + 1) != "workspace-write") return 14;
        } else if (args.contains("run")) {
            if (args.value(args.indexOf("--model") + 1) != "provider/model#high" || args.contains("--auto")) return 15;
        } else if (args.value(args.indexOf("--effort") + 1) != "high" || args.value(args.indexOf("--permission-mode") + 1) != "acceptEdits") return 16;
    }
    if (prompt.contains("CHECK_DEFAULT_EFFORT") && (args.contains("-c") || args.contains("--effort") || args.contains("provider/model#high") || args.contains("--sandbox") || args.contains("--permission-mode"))) return 17;
    QByteArray text = args.contains("--output-schema") || args.contains("--json-schema") || prompt.contains("JSON Schema") ? "{\"result\":\"ok\"}" : "hello";
    if (prompt.contains("BAD_SCHEMA")) text = "{\"result\":12}";
    if (prompt.contains("RESPONSE_TWO")) text = "two";
    if (prompt.contains("COUNT_TO_3")) {
        QFile counter("counter.txt"); int count = 0;
        if (counter.open(QIODevice::ReadOnly)) { count = counter.readAll().toInt(); counter.close(); }
        if (!counter.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 12;
        ++count; counter.write(QByteArray::number(count));
        text = "{\"count\":" + QByteArray::number(count) + "}";
    }
    auto match = QRegularExpression("PASS_COUNT=(\\d+)").match(QString::fromUtf8(prompt));
    if (match.hasMatch()) text = "{\"count\":" + match.captured(1).toUtf8() + "}";
    if (args.contains("exec")) {
        auto idx = args.indexOf("--output-last-message");
        QFile f(args.value(idx + 1)); if (!f.open(QIODevice::WriteOnly)) return 9; f.write(text); f.close();
        std::puts("{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"hello\"}}");
    } else if (args.contains("run")) {
        if (text.startsWith('{')) std::puts("{\"type\":\"text\",\"part\":{\"text\":\"{\\\"result\\\":\\\"ok\\\"}\"}}");
        else std::puts("{\"type\":\"text\",\"part\":{\"text\":\"hello\"}}");
    } else {
        if (text.startsWith('{')) std::puts("{\"type\":\"result\",\"is_error\":false,\"structured_output\":{\"result\":\"ok\"}}");
        else std::puts("{\"type\":\"result\",\"is_error\":false,\"result\":\"hello\"}");
    }
    std::fflush(stdout); return 0;
}
