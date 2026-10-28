#include "shared/constants.h"
#include "shared/strings.h"
#include "nodedefinition.h"
#include "flow.h"
#include <cmath>
#include <nlohmann/json-schema.hpp>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <functional>
#include <stdexcept>
#include <algorithm>

namespace an {
namespace {
std::string s(const QString &v) { return v.toUtf8().toStdString(); }
QString q(const std::string &v) { return QString::fromUtf8(v); }
[[noreturn]] void error(const QString &message) { throw std::runtime_error(s(message)); }
struct Part { QString key; bool index; };
std::vector<Part> parts(QString path) {
    std::vector<Part> result;
    QRegularExpression token(strings::VariablePathPattern);
    auto matches = token.globalMatch(path);
    qsizetype end = 0;
    while (matches.hasNext()) {
        auto m = matches.next();
        if (m.capturedStart() != end) error(strings::InvalidVariablePath + path);
        result.push_back({m.captured(1).isEmpty() ? m.captured(2) : m.captured(1), m.captured(1).isEmpty()});
        end = m.capturedEnd();
    }
    if (end != path.size() || result.empty()) error(strings::InvalidVariablePath + path);
    return result;
}
Json outputSchema(const Flow &f, Id id, QSet<Id> &visiting) {
    if (visiting.contains(id) || !f.nodes.contains(id)) return Json();
    visiting.insert(id);
    const auto &n = f.nodes.at(id);
    const auto &definition = nodeDefinition(n.kind);
    SchemaInputs inputs;
    if (definition.schemaUsesInputs) {
        std::vector<std::vector<Json>> ports(f.inputCount(n));
        std::vector<Json> incoming;
        for (const auto &e : f.edges) if (e.to == id) {
            auto schema = outputSchema(f, e.from, visiting);
            incoming.push_back(schema);
            if (e.input < ports.size()) ports[e.input].push_back(std::move(schema));
        }
        for (const auto &port : ports) inputs.ports.push_back(schemaSources(port));
        inputs.merged = schemaSources(incoming);
    }
    Json out = definition.schema(n, inputs);
    visiting.remove(id);
    return out;
}
}
bool isHostOperation(Kind k) { return nodeDefinition(k).host; }
bool hasNodeName(Kind k) { return nodeDefinition(k).named; }
QString kindName(Kind k) { return nodeDefinition(k).name; }
QString kindKey(Kind k) { return nodeDefinition(k).key; }
Kind parseKind(const std::string &key) {
    const auto value = QString::fromStdString(key);
    for (const auto &definition : nodeDefinitions())
        if (definition.key == value || definition.aliases.contains(value)) return definition.kind;
    error(strings::UnknownNodeType + value);
}
unsigned Flow::inputCount(const Node &n) const { return nodeDefinition(n.kind).inputs(n); }
unsigned Flow::outputCount(const Node &n) const { return nodeDefinition(n.kind).outputs(n); }
Json Flow::toJson() const {
    Json ns = Json::array(), es = Json::array();
    for (auto &[id, n] : nodes) {
        Json rules = Json::array();
        Json choices = Json::array(); for (const auto &choice : n.userChoices) choices.push_back(s(choice));
        for (auto &r : n.rules) rules.push_back({{strings::Variable, s(r.variable)}, {strings::Value, s(r.value)}, {strings::Equals, r.equals}});
        ns.push_back({{strings::Id, id}, {strings::Kind, s(kindKey(n.kind))}, {strings::Title, s(hasNodeName(n.kind) ? n.title : QString())},
            {strings::X, n.position.x()}, {strings::Y, n.position.y()}, {strings::Agent, s(n.agent)}, {strings::Model, s(n.model)}, {strings::Effort, s(n.effort)},
            {strings::Prompt, s(n.prompt)}, {strings::Schema, s(n.schema)}, {strings::RetainContext, n.retainContext}, {strings::Inputs, n.inputs}, {strings::Rules, rules},
            {strings::ScriptLanguage, s(n.scriptLanguage)}, {strings::ScriptPath, s(n.scriptPath)}, {strings::ScriptDirectory, s(n.scriptDirectory)},
            {strings::ShellCommand, s(n.shellCommand)}, {strings::ShellDirectory, s(n.shellDirectory)}, {strings::ShellOutput, s(n.shellOutput)}, {strings::TargetFlow, s(n.targetFlow)}, {strings::ProjectPath, s(n.projectPath)}, {strings::UserChoices, choices},
            {strings::WaitDuration, n.waitDuration}, {strings::WaitUnit, s(n.waitUnit)}, {strings::ScheduleMonth, s(n.scheduleMonth)}, {strings::ScheduleDay, s(n.scheduleDay)}, {strings::ScheduleHour, s(n.scheduleHour)}, {strings::ScheduleMinute, s(n.scheduleMinute)}, {strings::ScheduleWeekdays, n.scheduleWeekdays}});
    }
    for (auto &e : edges) es.push_back({{strings::From, e.from}, {strings::To, e.to}, {strings::Output, e.output}, {strings::Input, e.input}});
    const auto next = qMax(nextNodeId, nodes.empty() ? quint64(1) : quint64(nodes.rbegin()->first) + 1);
    return {{strings::Version, defaults::FlowVersion}, {strings::Name, s(name)}, {strings::Nodes, ns}, {strings::Edges, es}, {strings::NextNodeId, next}};
}
Flow Flow::fromJson(const Json &j) {
    if (j.at(strings::Version) != defaults::FlowVersion) error(strings::UnsupportedFlowVersion);
    Flow f; f.name = q(j.at(strings::Name).get<std::string>()); f.nextNodeId = j.value(strings::NextNodeId, quint64(1));
    for (auto &v : j.at(strings::Nodes)) {
        Node n; n.id = v.at(strings::Id).get<Id>(); n.kind = parseKind(v.at(strings::Kind));
        n.title = q(v.at(strings::Title)); n.position = {v.at(strings::X).get<double>(), v.at(strings::Y).get<double>()};
        if (!hasNodeName(n.kind)) n.title.clear();
        n.agent = q(v.value(strings::Agent, strings::Codex)); n.model = q(v.value(strings::Model, strings::Empty));
        n.effort = q(v.value(strings::Effort, strings::Empty));
        n.prompt = q(v.value(strings::Prompt, strings::Empty)); n.schema = q(v.value(strings::Schema, strings::Empty));
        n.retainContext = v.value(strings::RetainContext, false);
        n.inputs = v.value(strings::Inputs, defaults::InputPorts);
        n.scriptLanguage = q(v.value(strings::ScriptLanguage, strings::PythonLanguage)); n.scriptPath = q(v.value(strings::ScriptPath, strings::Empty)); n.scriptDirectory = q(v.value(strings::ScriptDirectory, strings::Empty));
        n.shellCommand = q(v.value(strings::ShellCommand, strings::Empty)); n.shellDirectory = q(v.value(strings::ShellDirectory, strings::Empty));
        n.shellOutput = q(v.value(strings::ShellOutput, strings::JsonFormat));
        const auto duration = v.value(strings::WaitDuration, double(defaults::WaitDuration));
        if (!std::isfinite(duration) || duration < 0 || duration > limits::WaitDuration || std::floor(duration) != duration) error(QString(strings::InvalidWaitDuration).arg(limits::WaitDuration));
        n.waitDuration = int(duration); n.waitUnit = q(v.value(strings::WaitUnit, strings::Seconds));
        n.scheduleMonth = q(v.value(strings::ScheduleMonth, strings::Wildcard)); n.scheduleDay = q(v.value(strings::ScheduleDay, strings::Wildcard)); n.scheduleHour = q(v.value(strings::ScheduleHour, strings::Wildcard)); n.scheduleMinute = q(v.value(strings::ScheduleMinute, strings::Wildcard));
        n.scheduleWeekdays = v.value(strings::ScheduleWeekdays, std::vector<int>{});
        n.targetFlow = q(v.value(strings::TargetFlow, strings::Empty)); n.projectPath = q(v.value(strings::ProjectPath, strings::Empty));
        if (v.contains(strings::UserChoices)) {
            if (!v.at(strings::UserChoices).is_array()) error(strings::UserChoicesMustBeArray);
            n.userChoices.clear(); for (const auto &choice : v.at(strings::UserChoices)) n.userChoices.push_back(q(choice.get<std::string>()));
        }
        if (n.inputs > limits::InputPorts) error(QString(strings::InputCountTooLarge).arg(limits::InputPorts));
        for (auto &r : v.at(strings::Rules)) n.rules.push_back({q(r.at(strings::Variable)), q(r.at(strings::Value)), r.at(strings::Equals).get<bool>()});
        if (!f.nodes.emplace(n.id, n).second) error(strings::DuplicateNodeId);
        f.nextNodeId = qMax(f.nextNodeId, quint64(n.id) + 1);
    }
    for (auto &e : j.at(strings::Edges)) f.edges.push_back({e.at(strings::From), e.at(strings::To), e.at(strings::Output), e.at(strings::Input)});
    return f;
}
Flow Flow::initial(bool withEnd) {
    Flow f;
    Node a; a.id = 1; a.kind = Kind::Start; a.title = kindName(a.kind); a.position = {0, metrics::InitialNodeY};
    Node b; b.id = 2; b.kind = Kind::End; b.title = kindName(b.kind); b.position = {metrics::InitialEndX, metrics::InitialNodeY};
    f.nodes.emplace(a.id, a); if (withEnd) f.nodes.emplace(b.id, b); return f;
}
void Flow::save(const QString &path) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) error(file.errorString());
    QByteArray data = QByteArray::fromStdString(toJson().dump(defaults::JsonIndent));
    if (file.write(data) != data.size() || !file.commit()) error(file.errorString());
}
Flow Flow::load(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) error(file.errorString());
    auto flow = fromJson(Json::parse(file.readAll().toStdString())); flow.sourcePath = QFileInfo(path).absoluteFilePath(); return flow;
}
Json parseJson(const QString &text) { return Json::parse(s(text)); }
QString display(const Json &v) { return v.is_string() ? q(v.get<std::string>()) : q(v.dump(defaults::JsonIndent)); }
Json resolve(const Json &value, const QString &path) {
    if (path == strings::WholeInput) return value;
    Json current = value;
    for (auto &p : parts(path)) {
        if (p.index) {
            bool ok = false; auto i = p.key.toULongLong(&ok);
            if (!ok || !current.is_array() || i >= current.size()) error(strings::MissingArrayElement + path);
            current = Json(current.at(size_t(i)));
        } else {
            if (!current.is_object() || !current.contains(s(p.key))) error(strings::MissingVariable + path);
            current = Json(current.at(s(p.key)));
        }
    }
    return current;
}
QString interpolate(const QString &text, const Json &value) {
    QRegularExpression re(strings::InterpolationPattern); auto it = re.globalMatch(text);
    QString out; qsizetype last = 0;
    while (it.hasNext()) { auto m = it.next(); out += text.mid(last, m.capturedStart() - last) + display(resolve(value, m.captured(1))); last = m.capturedEnd(); }
    return out + text.mid(last);
}
void checkSchema(const Json &schema) {
    // Validate both the schema document and references, using Draft 7.
    if (schema.is_object() && schema.contains(strings::SchemaDialect)) {
        const auto dialect = schema.at(strings::SchemaDialect).get<std::string>();
        if (dialect != strings::Draft7Http &&
            dialect != strings::Draft7Https &&
            dialect != strings::HttpJsonSchemaOrgDraft07Schema &&
            dialect != strings::HttpsJsonSchemaOrgDraft07Schema)
            error(strings::UnsupportedSchemaDialect);
    }
    // Draft 7 permits format to be annotation-only. Network URI resolution is
    // deliberately disabled; local $ref and structural assertions are checked.
    auto annotation = [](const std::string &, const std::string &) {};
    nlohmann::json_schema::json_validator meta(nullptr, annotation);
    meta.set_root_schema(nlohmann::json_schema::draft7_schema_builtin);
    meta.validate(schema);
    nlohmann::json_schema::json_validator validator(nullptr, annotation);
    validator.set_root_schema(schema);
}
void validateOutput(const Json &schema, const Json &output) {
    nlohmann::json_schema::json_validator validator(nullptr, [](const std::string &, const std::string &) {});
    validator.set_root_schema(schema); validator.validate(output);
}
Json schemaAt(const Json &schema, const QString &path) {
    if (path == strings::WholeInput) return schema;
    const auto segments = parts(path);
    std::function<Json(Json, const Json &, size_t)> walk = [&](Json current, const Json &root, size_t index) -> Json {
        // Script output is dynamic: validate variable syntax here, its value at execution.
        if (current.is_object() && current.contains(strings::AnUnknown) && current[strings::AnUnknown] == true) return current;
        if (current.is_object() && current.contains(strings::AnSources)) {
            std::vector<Json> alternatives;
            for (auto &source : current.at(strings::AnSources)) alternatives.push_back(walk(source, source, index));
            return schemaSources(alternatives);
        }
        if (index == segments.size()) return current;
        const auto &p = segments[index];
        // Resolve local references without allowing network schema loads.
        QSet<QString> refs;
        while (current.is_object() && current.contains(strings::SchemaReference)) {
            auto ref = q(current.at(strings::SchemaReference).get<std::string>());
            if (!ref.startsWith('#') || refs.contains(ref)) error(strings::UnresolvedSchemaReference + ref);
            refs.insert(ref); current = Json(root.at(Json::json_pointer(s(ref.mid(1)))));
        }
        if (p.index) {
            if (!current.is_object() || !current.contains(strings::Items)) error(strings::MissingSchemaItems + path);
            auto items = current.at(strings::Items);
            if (items.is_array()) {
                bool ok; auto i = p.key.toULongLong(&ok);
                if (!ok || i >= items.size()) error(strings::SchemaIndexOutOfRange + path);
                current = items.at(size_t(i));
            } else current = items;
        } else {
            if (!current.is_object() || !current.contains(strings::Properties) || !current[strings::Properties].contains(s(p.key))) error(strings::MissingSchemaVariable + path);
            current = Json(current.at(strings::Properties).at(s(p.key)));
        }
        return walk(current, root, index + 1);
    };
    return walk(schema, schema, 0);
}
QStringList Flow::validate() const {
    QStringList errors; int starts = 0, ends = 0;
    for (auto &[id, n] : nodes) { starts += nodeDefinition(n.kind).source; ends += nodeDefinition(n.kind).terminal; }
    if (starts != 1) errors << strings::MissingStart;
    if (ends > 1) errors << strings::TooManyEnds;
    std::map<std::pair<Id, unsigned>, unsigned> incoming;
    for (auto &e : edges) {
        if (!nodes.contains(e.from) || !nodes.contains(e.to)) { errors << strings::MissingConnectionNode; continue; }
        if (e.output >= outputCount(nodes.at(e.from)) || e.input >= inputCount(nodes.at(e.to))) errors << strings::InvalidConnectionPort;
        ++incoming[{e.to, e.input}];
    }
    for (size_t i = 0; i < edges.size(); ++i)
        for (size_t j = 0; j < i; ++j) if (edges[i] == edges[j]) errors << strings::DuplicateConnection;
    QSet<Id> reachable, terminal;
    std::function<void(Id, bool)> trace = [&](Id id, bool reverse) {
        auto &seen = reverse ? terminal : reachable;
        if (seen.contains(id)) return; seen.insert(id);
        for (auto &e : edges) if ((reverse ? e.to : e.from) == id) trace(reverse ? e.from : e.to, reverse);
    };
    for (auto &[id, n] : nodes) { if (nodeDefinition(n.kind).source) trace(id, false); if (nodeDefinition(n.kind).terminal) trace(id, true); }
    for (auto &[id, n] : nodes) {
        const auto label = QString(strings::NodeErrorPrefix).arg(n.title.isEmpty() ? kindName(n.kind) : n.title);
        if (!reachable.contains(id)) errors << label + strings::UnreachableFromStart;
        if (ends && !terminal.contains(id)) errors << label + strings::UnreachableEnd;
        for (unsigned p = 0; p < inputCount(n); ++p) if (!incoming.contains({id, p})) errors << label + QString(strings::UnconnectedInput).arg(p);
        const auto &definition = nodeDefinition(n.kind);
        for (const auto &message : definition.validate(*this, n)) errors << label + message;
        if (definition.outputConnectionsRequired) for (unsigned port = 0; port < outputCount(n); ++port) {
            const bool connected = std::any_of(edges.begin(), edges.end(), [&](const Edge &e) { return e.from == id && e.output == port; });
            if (!connected) errors << label + definition.unconnectedOutputMessage.arg(port);
        }
        try {
            std::vector<Json> inputs;
            for (auto &e : edges) if (e.to == id) { QSet<Id> seen; inputs.push_back(outputSchema(*this, e.from, seen)); }
            auto input = schemaSources(inputs);
            QRegularExpression re(strings::InterpolationPattern); auto it = re.globalMatch(definition.interpolation(n));
            while (it.hasNext()) schemaAt(input, it.next().captured(1));
            for (const auto &path : definition.variables(n)) schemaAt(input, path);
        } catch (const std::exception &e) { errors << label + QString::fromUtf8(e.what()); }
    }
    errors.removeDuplicates(); return errors;
}
}
