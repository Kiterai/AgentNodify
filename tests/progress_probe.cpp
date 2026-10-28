// Opt-in replay of locally captured reports; no CLI invocation or inference.
#include "core/progress.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

using namespace an;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); if (argc < 2) return 2;
    bool ok = true;
    for (const auto &path : app.arguments().mid(1)) {
        try {
            QFile file(path); if (!file.open(QIODevice::ReadOnly)) return 2;
            const auto report = Json::parse(file.readAll().toStdString());
            size_t messages = 0, tools = 0, accesses = 0, simple = 0, thinking = 0, commands = 0, boundaries = 0;
            const auto nodes = report.contains("nodes") ? report.at("nodes") : Json::array({report});
            for (const auto &node : nodes) {
                const auto log = node.value("log", std::string()); if (log.empty()) continue;
                ProgressParser parser; parser.append(QString::fromStdString(log)); parser.finish(State::Succeeded);
                for (const auto &entry : parser.items()) {
                    if (entry.kind == ProgressKind::Message) ++messages;
                    else if (entry.kind == ProgressKind::Tool) ++tools;
                    else if (entry.kind == ProgressKind::FileAccess) ++accesses;
                    else if (entry.kind == ProgressKind::Thinking) ++thinking;
                    else ++simple;
                    if (!entry.commandLabel.isEmpty()) ++commands;
                    if (entry.spaceBefore) ++boundaries;
                }
            }
            ok = ok && messages + tools + thinking > 0;
            QTextStream(stdout) << QFileInfo(path).fileName() << ": messages=" << messages << " tools=" << tools << " files=" << accesses << " simple=" << simple
                << " thinking=" << thinking << " commands=" << commands << " boundaries=" << boundaries << '\n';
        } catch (...) { QTextStream(stderr) << "Could not replay " << QFileInfo(path).fileName() << '\n'; return 1; }
    }
    return ok ? 0 : 1;
}
