// Explicit catalog-only integration check: never submits a prompt.
#include "core/modelcatalog.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QSaveFile>
#include <QTextStream>

using namespace an;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); QCommandLineParser parser; parser.addHelpOption();
    parser.addOptions({
        {{"a", "agent"}, "Codex / Claude Code / OpenCode", "agent"},
        {{"p", "program"}, "Native CLI executable", "path"},
        {{"r", "report"}, "Write model metadata only (no initialization/account payload)", "path"}
    }); parser.process(app);
    const auto agent = parser.value("agent");
    if ((agent != "Codex" && agent != "Claude Code" && agent != "OpenCode") || parser.value("program").isEmpty()) return 2;
    AgentConfig config; config.codex = config.claude = config.opencode = parser.value("program"); config.opencodeStandalone = true;
    ModelCatalog catalog; catalog.setConfig(config);
    QObject::connect(&catalog, &ModelCatalog::changed, &app, [&](const QString &changed) {
        if (changed != agent) return; const auto entry = catalog.entry(agent);
        if (entry.state != CatalogState::Ready && entry.state != CatalogState::Error) return;
        bool passed = entry.state == CatalogState::Ready;
        Json models = Json::array();
        for (auto &model : entry.models) models.push_back({{"id", model.id.toStdString()}, {"name", model.name.toStdString()}, {"description", model.description.toStdString()}});
        Json report = {{"agent", agent.toStdString()}, {"passed", passed}, {"error", entry.error.toStdString()},
            {"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()}, {"models", models}};
        if (!parser.value("report").isEmpty()) {
            QSaveFile file(parser.value("report")); auto bytes = QByteArray::fromStdString(report.dump(2));
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) passed = false;
        }
        QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << agent << " catalog: " << entry.models.size() << " models" << (entry.error.isEmpty() ? "" : " · " + entry.error) << '\n';
        app.exit(passed ? 0 : 1);
    });
    QTimer::singleShot(0, &app, [&] { catalog.request(agent); }); return app.exec();
}
