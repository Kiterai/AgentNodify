#include "core/nodedefinition.h"
#include "ui/nodeform.h"
#include "ui/nodeparameterform.h"
#include "ui/nodepresentation.h"
#include "core/modelcatalog.h"
#include "shared/constants.h"
#include "shared/strings.h"
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

using namespace an;

class NodeRegistryTest : public QObject {
    Q_OBJECT
  private slots:
    void definitionsAreComplete() {
        const auto &definitions = nodeDefinitions();
        QCOMPARE(definitions.size(), size_t(Kind::Schedule) + 1);
        QCOMPARE(nodePresentations().size(), definitions.size());
        QSet<QString> keys;
        QSet<int> kinds;
        int sources = 0, terminals = 0;
        for (const auto &d : definitions) {
            QVERIFY(!d.key.isEmpty());
            QVERIFY(!d.name.isEmpty());
            const auto &presentation = nodePresentation(d.kind);
            QVERIFY(!presentation.description.isEmpty());
            QCOMPARE(presentation.kind, d.kind);
            QVERIFY(presentation.portCaption);
            QCOMPARE(bool(presentation.createForm), !d.source && !d.terminal);
            QVERIFY(!keys.contains(d.key));
            QVERIFY(!kinds.contains(int(d.kind)));
            keys.insert(d.key);
            kinds.insert(int(d.kind));
            QCOMPARE(parseKind(d.key.toStdString()), d.kind);
            QCOMPARE(&nodeDefinition(d.kind), &d);
            QVERIFY(d.execute);
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
    void parameterFormsAreOwnedAndIsolated() {
        ModelCatalog catalog;
        for (const auto &definition : nodeDefinitions()) {
            Node node; node.kind = definition.kind;
            auto *form = new NodeForm(node, nullptr, &catalog);
            QPointer<NodeParameterForm> parameters;
            int controllers = 0;
            for (auto *child : form->children()) {
                if (auto *controller = dynamic_cast<NodeParameterForm *>(child)) {
                    parameters = controller;
                    ++controllers;
                }
            }
            QCOMPARE(controllers, definition.source || definition.terminal ? 0 : 1);
            // Other kinds must not carry an agent's model widgets/context.
            QCOMPARE(form->findChild<QComboBox *>(strings::NodeAgent) != nullptr, definition.kind == Kind::Agent);
            QCOMPARE(form->findChild<QLineEdit *>(strings::ScriptPath) != nullptr, definition.kind == Kind::Script);
            QCOMPARE(form->findChild<QSpinBox *>(strings::WaitDuration) != nullptr, definition.kind == Kind::Delay);
            QCOMPARE(form->findChild<QComboBox *>(strings::TargetFlow) != nullptr, definition.host);
            if (auto *preview = form->findChild<TextPreview *>()) {
                QSignalSpy edits(form, &NodeForm::textEditRequested);
                emit preview->editRequested();
                QCOMPARE(edits.size(), 1);
            }
            delete form;
            QVERIFY(parameters.isNull());
            // Queued resize/catalog callbacks must not outlive their controller.
            emit catalog.changed(strings::Codex);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();
        }
    }
    void concreteFormsSynchronizeWithoutPublishingEdits() {
        for (const auto &definition : nodeDefinitions()) {
            Node node; node.kind = definition.kind;
            NodeForm form(node);
            QSignalSpy edits(&form, &NodeForm::edited);
            node.prompt = "updated prompt"; node.schema = "{}"; node.model = "fixture-model";
            node.scriptLanguage = strings::JavaScriptLanguage; node.scriptPath = "updated.js";
            node.shellCommand = "echo updated"; node.shellOutput = strings::TextFormat;
            node.inputs = 1; node.waitDuration = 2; node.waitUnit = strings::Hours;
            node.scheduleMonth = "2"; node.scheduleWeekdays = {1};
            node.userChoices = {"first", "second"}; node.rules = {{"result", "ok", true}};
            node.targetFlow = "fixture.flow.json"; node.projectPath = "project";
            form.setFlowChoices({{node.targetFlow, "fixture"}});
            form.setNode(node);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCOMPARE(edits.size(), 0);
            if (auto *edit = form.findChild<QLineEdit *>(strings::ScriptPath)) QCOMPARE(edit->text(), node.scriptPath);
            if (auto *edit = form.findChild<QLineEdit *>(strings::ScheduleMonth)) QCOMPARE(edit->text(), node.scheduleMonth);
            if (auto *spin = form.findChild<QSpinBox *>(strings::NodeInputs)) QCOMPARE(spin->value(), int(node.inputs));
            if (auto *spin = form.findChild<QSpinBox *>(strings::WaitDuration)) QCOMPARE(spin->value(), node.waitDuration);
            if (auto *combo = form.findChild<QComboBox *>(strings::NodeModel)) QCOMPARE(combo->currentData().toString(), node.model);
            if (auto *combo = form.findChild<QComboBox *>(strings::ShellOutput)) QCOMPARE(combo->currentData().toString(), node.shellOutput);
            if (auto *combo = form.findChild<QComboBox *>(strings::TargetFlow)) QCOMPARE(combo->currentData().toString(), node.targetFlow);
            if (definition.kind == Kind::UserInput) QCOMPARE(form.findChildren<QLineEdit *>(strings::ChoiceName).size(), node.userChoices.size());
            if (definition.kind == Kind::Branch) QCOMPARE(form.findChildren<QLineEdit *>(strings::RuleVariable).size(), qsizetype(node.rules.size()));
            form.setReadOnly(true);
            for (auto *edit : form.findChildren<QLineEdit *>()) QVERIFY(edit->isReadOnly());
            for (auto *check : form.findChildren<QCheckBox *>()) QVERIFY(!check->isEnabled());
            if (auto *browse = form.findChild<QPushButton *>(strings::ScriptFileBrowse)) QVERIFY(!browse->isEnabled());
            if (auto *add = form.findChild<QPushButton *>(strings::AddChoiceValue)) QVERIFY(!add->isEnabled());
            form.setReadOnly(false);
            for (auto *edit : form.findChildren<QLineEdit *>()) QVERIFY(!edit->isReadOnly());
            QCOMPARE(edits.size(), 0);
        }
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
            if (relative == "ui/nodeform.h") {
                for (const auto *specific : {"agent_", "scriptLanguage_", "shellOutput_", "targetFlow_", "waitDuration_",
                                             "scheduleWeekdays_", "choices_", "rules_", "populateModels", "rebuildRules", "buildAgent"})
                    QVERIFY2(!text.contains(specific), specific);
            }
            if (relative == "shared/strings.h" || relative == "shared/colors.h")
                continue;
            // Only the single loader imports per-kind implementation fragments.
            if (relative != "nodes/registry.cpp")
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
                if (relative != "shared/constants.h") {
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
            if (relative.startsWith("nodes/") && relative != "nodes/registry.cpp") {
                // No sibling node includes or dispatch to a different concrete kind.
                QVERIFY2(!text.contains("#include"), qPrintable(relative));
                const auto folder = relative.section('/', 1, 1);
                if (relative.endsWith("definition.inc")) {
                    QVERIFY2(text.contains("NodeDefinition NodeRegistry::"), qPrintable(relative));
                    QCOMPARE(text.count("Kind::"), 1);
                    QVERIFY2(source.exists("nodes/" + folder + "/core/definition.inc"), qPrintable(relative));
                    QVERIFY2(source.exists("nodes/" + folder + "/ui/presentation.inc"), qPrintable(relative));
                }
            }
        }
    }
};
QTEST_MAIN(NodeRegistryTest)
#include "node_registry_test.moc"
