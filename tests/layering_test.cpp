#include "layering_test.h"
#include "core/flow.h"
#include "core/nodedefinition.h"
#include "core/runner.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

// This target links only agentcore. An accidental transitive GUI dependency is
// a compile-time failure, not merely a source-layout convention.
#if !defined(Q_MOC_RUN) && (defined(QT_WIDGETS_LIB) || defined(QT_GUI_LIB))
#error The internal model must not depend on Qt GUI or Widgets
#endif

using namespace an;

void LayeringTest::coreRunsWithoutTheUi() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto flow = Flow::initial();
    flow.edges.push_back({1, 2});
    QVERIFY(flow.validate().isEmpty());
    QCOMPARE(Flow::fromJson(flow.toJson()).toJson(), flow.toJson());
    Runner runner;
    QSignalSpy finished(&runner, &Runner::finished);
    QVERIFY(runner.start(flow, directory.path(), AgentConfig{}));
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QVERIFY(finished.front().front().toBool());
    QCOMPARE(runner.results().at(2).state, State::Succeeded);
    QCOMPARE(runner.results().at(2).output, Json::object());
    QCOMPARE(nodeDefinitions().size(), size_t(Kind::Schedule) + 1);
}
void LayeringTest::lowerLayersNeverIncludeTheUi() {
    const QDir source(QStringLiteral(AGENTNODIFY_SOURCE_DIR) + "/src");
    const QRegularExpression includes(R"(^\s*#\s*include\s*["<]([^">]+)[">])", QRegularExpression::MultilineOption);
    const QRegularExpression uiSymbols(
        R"(\b(?:NodePresentation|NodeForm|NodeParameterForm|QWidget|QColor|QComboBox|QFormLayout|QVBoxLayout)\b)");
    QDirIterator files(source.path(), {"*.h", "*.cpp", "*.inc"}, QDir::Files, QDirIterator::Subdirectories);
    int coreDefinitions = 0;
    while (files.hasNext()) {
        const auto path = files.next();
        const auto relative = source.relativeFilePath(path);
        const bool core = relative.startsWith("core/") || (relative.startsWith("nodes/") && relative.contains("/core/"));
        const bool shared = relative.startsWith("shared/");
        if (!core && !shared)
            continue;
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(file.readAll());
        if (core) {
            QVERIFY2(!uiSymbols.match(text).hasMatch(), qPrintable(relative));
            QVERIFY2(!text.contains("colors::"), qPrintable(relative));
            if (relative.endsWith("/definition.inc"))
                ++coreDefinitions;
        }
        auto imports = includes.globalMatch(text);
        while (imports.hasNext()) {
            const auto include = imports.next().captured(1);
            QVERIFY2(!include.contains("QtWidgets") && !include.contains("QtGui"), qPrintable(relative + ": " + include));
            QFileInfo candidate(QDir(QFileInfo(path).absolutePath()).filePath(include));
            if (!candidate.exists())
                candidate = QFileInfo(source.filePath(include));
            // Qt and standard-library imports are checked by the build target.
            if (!candidate.exists())
                continue;
            const auto target = source.relativeFilePath(candidate.canonicalFilePath());
            QVERIFY2(target.startsWith("shared/") || (core && target.startsWith("core/")), qPrintable(relative + " -> " + target));
        }
    }
    QCOMPARE(coreDefinitions, int(nodeDefinitions().size()));
}
void LayeringTest::allSourcesHaveBuildReferences() {
    const QDir project(QStringLiteral(AGENTNODIFY_SOURCE_DIR));
    const QDir source(project.filePath("src"));
    QMap<QString, QString> contents;
    QDirIterator files(source.path(), {"*.h", "*.cpp", "*.inc"}, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto path = files.next();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        contents.insert(source.relativeFilePath(path), QString::fromUtf8(file.readAll()));
    }
    QFile cmake(project.filePath("CMakeLists.txt"));
    QVERIFY(cmake.open(QIODevice::ReadOnly));
    const auto build = QString::fromUtf8(cmake.readAll());
    const QRegularExpression sources(R"(\bsrc/([A-Za-z0-9_/.-]+\.(?:cpp|h|inc))\b)");
    const QRegularExpression includes(R"inc(^\s*#include\s+"([^"]+)")inc", QRegularExpression::MultilineOption);
    QStringList pending;
    auto roots = sources.globalMatch(build);
    while (roots.hasNext()) pending.push_back(roots.next().captured(1));
    QVERIFY(!pending.isEmpty());
    QSet<QString> reachable;
    while (!pending.isEmpty()) {
        const auto path = pending.takeLast();
        if (reachable.contains(path) || !contents.contains(path)) continue;
        reachable.insert(path);
        auto imports = includes.globalMatch(contents.value(path));
        while (imports.hasNext()) {
            const auto include = imports.next().captured(1);
            const auto local = QDir::cleanPath(QDir(QFileInfo(path).path()).filePath(include));
            if (contents.contains(local)) pending.push_back(local);
            else if (contents.contains(include)) pending.push_back(include);
        }
    }
    QStringList unused;
    for (auto it = contents.cbegin(); it != contents.cend(); ++it)
        if (!reachable.contains(it.key())) unused.push_back(it.key());
    QVERIFY2(unused.isEmpty(), qPrintable("Sources without CMake/include references: " + unused.join(", ")));
}
QTEST_GUILESS_MAIN(LayeringTest)
