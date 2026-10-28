#include "nodedefinition.h"
#include "nodeform.h"
#include "constants.h"
#include "strings.h"
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QtTest>

using namespace an;

class NodeRegistryTest : public QObject {
    Q_OBJECT
  private slots:
    void definitionsAreComplete() {
        const auto &definitions = nodeDefinitions();
        QCOMPARE(definitions.size(), size_t(Kind::Schedule) + 1);
        QSet<QString> keys;
        QSet<int> kinds;
        int sources = 0, terminals = 0;
        for (const auto &d : definitions) {
            QVERIFY(!d.key.isEmpty());
            QVERIFY(!d.name.isEmpty());
            QVERIFY(!d.description.isEmpty());
            QVERIFY(!keys.contains(d.key));
            QVERIFY(!kinds.contains(int(d.kind)));
            keys.insert(d.key);
            kinds.insert(int(d.kind));
            QCOMPARE(parseKind(d.key.toStdString()), d.kind);
            QCOMPARE(&nodeDefinition(d.kind), &d);
            QVERIFY(d.execute);
            QVERIFY(d.portCaption);
            sources += d.source;
            terminals += d.terminal;
            if (d.prepare)
                QVERIFY(d.decode);
            if (d.host) {
                QVERIFY(d.operate);
                QVERIFY(d.hostFailure);
            }
            if (d.editableOutputs) {
                QVERIFY(d.sameOutput);
                QVERIFY(d.editableOutputCount);
            }
            Node node;
            node.kind = d.kind;
            Flow flow;
            flow.nodes[node.id] = node;
            const auto restored = Flow::fromJson(flow.toJson());
            QCOMPARE(restored.nodes.at(node.id).kind, d.kind);
            QCOMPARE(restored.toJson(), flow.toJson());
        }
        QCOMPARE(sources, 1);
        QCOMPARE(terminals, 1);
        QCOMPARE(parseKind("and"), Kind::All);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseKind("not-a-node"));
    }
    void limitsAreSharedWithFormsAndLoading() {
        for (const auto kind : {Kind::All, Kind::Any}) {
            Node node;
            node.kind = kind;
            NodeForm form(node);
            const auto *spin = form.findChild<QSpinBox *>(strings::NodeInputs);
            QVERIFY(spin);
            QCOMPARE(spin->minimum(), 1);
            QCOMPARE(spin->maximum(), limits::InputPorts);
            Flow flow;
            node.inputs = limits::InputPorts;
            flow.nodes[node.id] = node;
            QCOMPARE(Flow::fromJson(flow.toJson()).nodes.at(node.id).inputs, unsigned(limits::InputPorts));
            flow.nodes[node.id].inputs = limits::InputPorts + 1;
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, Flow::fromJson(flow.toJson()));
        }
        Node node;
        node.kind = Kind::Delay;
        NodeForm form(node);
        const auto *spin = form.findChild<QSpinBox *>(strings::WaitDuration);
        QVERIFY(spin);
        QCOMPARE(spin->maximum(), limits::WaitDuration);
        Flow flow;
        node.waitDuration = limits::WaitDuration;
        flow.nodes[node.id] = node;
        QCOMPARE(Flow::fromJson(flow.toJson()).nodes.at(node.id).waitDuration, limits::WaitDuration);
        flow.nodes[node.id].waitDuration = limits::WaitDuration + 1;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Flow::fromJson(flow.toJson()));
    }
    void outputRemappingPreservesIdentity() {
        Node before;
        before.kind = Kind::Branch;
        before.rules = {{"x", "a", true}, {"x", "a", true}, {"x", "b", true}};
        auto after = before;
        after.rules.erase(after.rules.begin() + 1);
        QCOMPARE(remapNodeOutputs(before, after, 1), (std::vector<int>{0, -1, 1, 2}));
        before.kind = Kind::UserInput;
        before.userChoices = {"same", "same", "other"};
        after = before;
        after.userChoices.removeAt(1);
        QCOMPARE(remapNodeOutputs(before, after, 1), (std::vector<int>{0, -1, 1}));
    }
    void sourceBoundariesStayEnforced() {
        const QDir source(QStringLiteral(AGENTNODIFY_SOURCE_DIR) + "/src");
        QDirIterator files(source.path(), {"*.h", "*.cpp", "*.inc"}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto path = files.next();
            const auto relative = source.relativeFilePath(path);
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto text = QString::fromUtf8(file.readAll());
            if (relative == "strings.h" || relative == "colors.h")
                continue;
            // Only the single loader imports per-kind implementation fragments.
            if (relative != "noderegistry.cpp")
                QVERIFY2(!text.contains("#include \"nodes/"), qPrintable(relative));
            bool blockComment = false;
            for (const auto &line : text.split('\n')) {
                if (line.trimmed().startsWith("#include"))
                    continue;
                QString code;
                for (qsizetype i = 0; i < line.size(); ++i) {
                    if (blockComment) {
                        if (line.mid(i, 2) == "*/") {
                            blockComment = false;
                            ++i;
                        }
                        continue;
                    }
                    if (line.mid(i, 2) == "//")
                        break;
                    if (line.mid(i, 2) == "/*") {
                        blockComment = true;
                        ++i;
                        continue;
                    }
                    if (line[i] == '\'') { // Character literals are syntax, not UI strings.
                        while (++i < line.size()) {
                            if (line[i] == '\\')
                                ++i;
                            else if (line[i] == '\'')
                                break;
                        }
                        continue;
                    }
                    QVERIFY2(line[i] != '"', qPrintable(relative + ": inline string literal: " + line.trimmed()));
                    code += line[i];
                }
                if (relative != "constants.h") {
                    // Zero/one/two are syntax (indices, identity, halving, etc.).
                    // Domain limits, geometry, timings and defaults live in constants.h.
                    static const QRegularExpression number(R"(\b(0x[0-9a-fA-F]+|[0-9]+(?:\.[0-9]+)?)(?:[uUfF])?\b)");
                    auto matches = number.globalMatch(code);
                    while (matches.hasNext()) {
                        const auto value = matches.next().captured(1);
                        QVERIFY2(value == "0" || value == "1" || value == "2", qPrintable(relative + ": inline number: " + value));
                    }
                }
            }
            if (relative.startsWith("nodes/")) {
                // No sibling node includes or dispatch to a different concrete kind.
                QVERIFY2(!text.contains("#include"), qPrintable(relative));
                const auto folder = relative.section('/', 1, 1);
                if (relative.endsWith("definition.inc")) {
                    QVERIFY2(text.contains("NodeDefinition NodeRegistry::"), qPrintable(relative));
                    QCOMPARE(text.count("Kind::"), relative.startsWith("nodes/start/") || relative.startsWith("nodes/end/") ? 2 : 1);
                    QVERIFY2(source.exists("nodes/" + folder + "/definition.inc"), qPrintable(relative));
                }
            }
        }
    }
};
QTEST_MAIN(NodeRegistryTest)
#include "node_registry_test.moc"
