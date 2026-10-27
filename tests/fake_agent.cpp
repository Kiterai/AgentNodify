// Deterministic CLI protocol fixture, never calls an AI service.
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QLockFile>
#include <cstdio>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); auto args = app.arguments();
    QFile input; if (!input.open(stdin, QIODevice::ReadOnly)) return 11; QByteArray prompt;
    if (!args.contains("run")) prompt = input.readAll();
    else prompt = args.last().toUtf8();
    if(prompt.startsWith("CHECK_WHOLE_INPUT\n")) {
        QFile expected("whole-input.txt"); if(!expected.open(QIODevice::ReadOnly) || prompt.mid(QByteArray("CHECK_WHOLE_INPUT\n").size())!=expected.readAll()) return 30;
    }
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
    if (args.contains("exec") && args.value(args.indexOf("--sandbox") + 1) != "workspace-write") return 21;
    if (!args.contains("exec") && !args.contains("run") && args.value(args.indexOf("--permission-mode") + 1) != "acceptEdits") return 22;
    if (args.contains("--dangerously-skip-permissions") || args.contains("--dangerously-bypass-approvals-and-sandbox") || args.contains("--auto")) return 23;
    if (prompt.contains("CHECK_EFFORT_HIGH")) {
        if (args.contains("exec")) {
            if (args.value(args.indexOf("-c") + 1) != "model_reasoning_effort=\"high\"" || args.value(args.indexOf("--sandbox") + 1) != "workspace-write") return 14;
        } else if (args.contains("run")) {
            if (args.value(args.indexOf("--model") + 1) != "provider/model#high" || args.contains("--auto")) return 15;
        } else if (args.value(args.indexOf("--effort") + 1) != "high" || args.value(args.indexOf("--permission-mode") + 1) != "acceptEdits") return 16;
    }
    if (prompt.contains("CHECK_DEFAULT_EFFORT") && (args.contains("-c") || args.contains("--effort") || args.contains("provider/model#high"))) return 17;
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
    if (args.contains("--last") || args.contains("--continue") || (args.contains("-c") && !args.contains("exec"))) return 24;
    const auto resumeFlag = args.contains("exec") ? "resume" : args.contains("run") ? "--session" : "--resume";
    const bool resumed = args.contains(resumeFlag);
    QString session = resumed ? args.value(args.indexOf(resumeFlag) + 1) : QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (args.contains("run") && !resumed) session = "ses_" + session.remove('-');
    if (prompt.contains("CONTEXT_FIXTURE")) {
        if (resumed && prompt.contains("RESUME_ERROR")) return 27;
        QFile state("ctx_" + session + ".json"); int count = 0;
        if (resumed) { if (!state.open(QIODevice::ReadOnly)) return 25; count = state.readAll().toInt(); state.close(); }
        if (!state.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 26;
        state.write(QByteArray::number(++count)); state.close(); text = "{\"count\":" + QByteArray::number(count) + "}";
        {
            // The isolation fixture launches several processes in one directory.
            // Serialize trace appends so the test's own log cannot lose a line.
            QLockFile traceLock("context-trace.lock"); if (!traceLock.tryLock(5000)) return 29;
            QFile trace("context-trace.jsonl"); if (!trace.open(QIODevice::WriteOnly | QIODevice::Append)) return 28;
            trace.write(QJsonDocument(QJsonObject{{"session", session}, {"resumed", resumed}, {"count", count}, {"args", QJsonArray::fromStringList(args)}}).toJson(QJsonDocument::Compact) + '\n');
        }
        if (resumed && prompt.contains("CANCEL_DELAY")) QThread::sleep(5);
        if (resumed && prompt.contains("CHANGE_SESSION")) session = args.contains("run") ? "ses_changed" : "00000000-0000-0000-0000-000000000001";
    }
    auto emitEvent = [](const QJsonObject &event) { const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact); std::puts(bytes.constData()); };
    if (!prompt.contains("OMIT_SESSION")) {
        if (args.contains("exec")) emitEvent({{"type", "thread.started"}, {"thread_id", session}});
        else if (args.contains("run")) emitEvent({{"type", "step_start"}, {"sessionID", session}});
        else emitEvent({{"type", "system"}, {"subtype", "init"}, {"session_id", session}});
    }
    if (args.contains("exec")) {
        auto idx = args.indexOf("--output-last-message");
        QFile f(args.value(idx + 1)); if (!f.open(QIODevice::WriteOnly)) return 9; f.write(text); f.close();
        std::puts("{\"type\":\"item.completed\",\"item\":{\"type\":\"agent_message\",\"text\":\"hello\"}}");
    } else if (args.contains("run")) {
        emitEvent({{"type", "text"}, {"part", QJsonObject{{"text", QString::fromUtf8(text)}}}});
    } else {
        if (text.startsWith('{') && args.contains("--json-schema")) emitEvent({{"type", "result"}, {"is_error", false}, {"structured_output", QJsonDocument::fromJson(text).object()}});
        else emitEvent({{"type", "result"}, {"is_error", false}, {"result", QString::fromUtf8(text)}});
    }
    std::fflush(stdout); return 0;
}
