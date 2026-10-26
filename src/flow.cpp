#include "flow.h"
#include <nlohmann/json-schema.hpp>
#include <QFile>
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
    QRegularExpression token(R"((?:^|\.)([A-Za-z_\p{L}][\w\p{L}-]*)|\[(\d+)\])");
    auto matches = token.globalMatch(path);
    qsizetype end = 0;
    while (matches.hasNext()) {
        auto m = matches.next();
        if (m.capturedStart() != end) error("不正な変数パス: " + path);
        result.push_back({m.captured(1).isEmpty() ? m.captured(2) : m.captured(1), m.captured(1).isEmpty()});
        end = m.capturedEnd();
    }
    if (end != path.size() || result.empty()) error("不正な変数パス: " + path);
    return result;
}
// Internal alternatives preserve each producer's own $ref root. This is only
// used for variable-path inference, never sent as a schema to an agent.
Json sources(const std::vector<Json> &values) {
    Json alternatives = Json::array();
    for (auto &value : values) if (!value.is_null() && std::find(alternatives.begin(), alternatives.end(), value) == alternatives.end()) alternatives.push_back(value);
    return alternatives.empty() ? Json() : Json{{"_anSources", alternatives}};
}
Json outputSchema(const Flow &f, Id id, QSet<Id> &visiting) {
    if (visiting.contains(id) || !f.nodes.contains(id)) return Json();
    visiting.insert(id);
    const auto &n = f.nodes.at(id);
    Json out;
    if (n.kind == Kind::Start) out = {{"type", "object"}, {"properties", Json::object()}, {"additionalProperties", false}};
    else if (n.kind == Kind::Agent) out = n.schema.trimmed().isEmpty() ? Json{{"type", "string"}} : parseJson(n.schema);
    else if (n.kind == Kind::And) {
        Json items = Json::array();
        for (unsigned i = 0; i < n.inputs; ++i) {
            std::vector<Json> incoming;
            for (auto &e : f.edges) if (e.to == id && e.input == i) incoming.push_back(outputSchema(f, e.from, visiting));
            items.push_back(sources(incoming));
        }
        out = {{"type", "array"}, {"items", items}};
    } else {
        std::vector<Json> incoming;
        for (auto &e : f.edges) if (e.to == id) incoming.push_back(outputSchema(f, e.from, visiting));
        out = sources(incoming);
    }
    visiting.remove(id);
    return out;
}
}
QString kindName(Kind k) {
    switch (k) {
    case Kind::Start: return QStringLiteral("始端");
    case Kind::End: return QStringLiteral("終端");
    case Kind::And: return QStringLiteral("AND");
    case Kind::Branch: return QStringLiteral("分岐");
    default: return QStringLiteral("エージェント");
    }
}
QString kindKey(Kind k) {
    switch(k) { case Kind::Start: return "start"; case Kind::End: return "end";
    case Kind::And: return "and"; case Kind::Branch: return "branch"; default: return "agent"; }
}
Kind parseKind(const std::string &k) {
    if (k == "start") return Kind::Start;
    if (k == "end") return Kind::End;
    if (k == "and") return Kind::And;
    if (k == "branch") return Kind::Branch;
    if (k == "agent") return Kind::Agent;
    error("不明なノード種別: " + q(k));
}
unsigned Flow::inputCount(const Node &n) const { return n.kind == Kind::Start ? 0 : n.kind == Kind::And ? n.inputs : 1; }
unsigned Flow::outputCount(const Node &n) const { return n.kind == Kind::End ? 0 : n.kind == Kind::Branch ? unsigned(n.rules.size()) + 1 : 1; }
Json Flow::toJson() const {
    Json ns = Json::array(), es = Json::array();
    for (auto &[id, n] : nodes) {
        Json rules = Json::array();
        for (auto &r : n.rules) rules.push_back({{"variable", s(r.variable)}, {"value", s(r.value)}, {"equals", r.equals}});
        ns.push_back({{"id", id}, {"kind", s(kindKey(n.kind))}, {"title", s(n.title)},
            {"x", n.position.x()}, {"y", n.position.y()}, {"agent", s(n.agent)}, {"model", s(n.model)},
            {"prompt", s(n.prompt)}, {"schema", s(n.schema)}, {"inputs", n.inputs}, {"rules", rules}});
    }
    for (auto &e : edges) es.push_back({{"from", e.from}, {"to", e.to}, {"output", e.output}, {"input", e.input}});
    return {{"version", 1}, {"name", s(name)}, {"nodes", ns}, {"edges", es}};
}
Flow Flow::fromJson(const Json &j) {
    if (j.at("version") != 1) error("未対応のフローファイル形式です");
    Flow f; f.name = q(j.at("name").get<std::string>());
    for (auto &v : j.at("nodes")) {
        Node n; n.id = v.at("id").get<Id>(); n.kind = parseKind(v.at("kind"));
        n.title = q(v.at("title")); n.position = {v.at("x").get<double>(), v.at("y").get<double>()};
        n.agent = q(v.value("agent", "Codex")); n.model = q(v.value("model", ""));
        n.prompt = q(v.value("prompt", "")); n.schema = q(v.value("schema", ""));
        n.inputs = v.value("inputs", 2u);
        if (n.inputs > 256) error("AND入力数は256以下にしてください");
        for (auto &r : v.at("rules")) n.rules.push_back({q(r.at("variable")), q(r.at("value")), r.at("equals").get<bool>()});
        if (!f.nodes.emplace(n.id, n).second) error("ノードIDが重複しています");
    }
    for (auto &e : j.at("edges")) f.edges.push_back({e.at("from"), e.at("to"), e.at("output"), e.at("input")});
    return f;
}
Flow Flow::initial() {
    Flow f;
    Node a; a.id = 1; a.kind = Kind::Start; a.title = kindName(a.kind); a.position = {0, 120};
    Node b; b.id = 2; b.kind = Kind::End; b.title = kindName(b.kind); b.position = {600, 120};
    f.nodes.emplace(a.id, a); f.nodes.emplace(b.id, b); return f;
}
void Flow::save(const QString &path) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) error(file.errorString());
    QByteArray data = QByteArray::fromStdString(toJson().dump(2));
    if (file.write(data) != data.size() || !file.commit()) error(file.errorString());
}
Flow Flow::load(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) error(file.errorString());
    return fromJson(Json::parse(file.readAll().toStdString()));
}
Json parseJson(const QString &text) { return Json::parse(s(text)); }
QString display(const Json &v) { return v.is_string() ? q(v.get<std::string>()) : q(v.dump(2)); }
Json resolve(const Json &value, const QString &path) {
    Json current = value;
    for (auto &p : parts(path)) {
        if (p.index) {
            bool ok = false; auto i = p.key.toULongLong(&ok);
            if (!ok || !current.is_array() || i >= current.size()) error("配列要素が存在しません: " + path);
            current = Json(current.at(size_t(i)));
        } else {
            if (!current.is_object() || !current.contains(s(p.key))) error("変数が存在しません: " + path);
            current = Json(current.at(s(p.key)));
        }
    }
    return current;
}
QString interpolate(const QString &text, const Json &value) {
    QRegularExpression re(R"(\$\{([^}]+)\})"); auto it = re.globalMatch(text);
    QString out; qsizetype last = 0;
    while (it.hasNext()) { auto m = it.next(); out += text.mid(last, m.capturedStart() - last) + display(resolve(value, m.captured(1))); last = m.capturedEnd(); }
    return out + text.mid(last);
}
void checkSchema(const Json &schema) {
    // Validate both the schema document and references, using Draft 7.
    if (schema.is_object() && schema.contains("$schema")) {
        const auto dialect = schema.at("$schema").get<std::string>();
        if (dialect != "http://json-schema.org/draft-07/schema#" &&
            dialect != "https://json-schema.org/draft-07/schema#" &&
            dialect != "http://json-schema.org/draft-07/schema" &&
            dialect != "https://json-schema.org/draft-07/schema")
            error("対応するJSON SchemaはDraft 7です");
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
    const auto segments = parts(path);
    std::function<Json(Json, const Json &, size_t)> walk = [&](Json current, const Json &root, size_t index) -> Json {
        if (current.is_object() && current.contains("_anSources")) {
            std::vector<Json> alternatives;
            for (auto &source : current.at("_anSources")) alternatives.push_back(walk(source, source, index));
            return sources(alternatives);
        }
        if (index == segments.size()) return current;
        const auto &p = segments[index];
        // Resolve local references without allowing network schema loads.
        QSet<QString> refs;
        while (current.is_object() && current.contains("$ref")) {
            auto ref = q(current.at("$ref").get<std::string>());
            if (!ref.startsWith('#') || refs.contains(ref)) error("解決できないスキーマ参照: " + ref);
            refs.insert(ref); current = Json(root.at(Json::json_pointer(s(ref.mid(1)))));
        }
        if (p.index) {
            if (!current.is_object() || !current.contains("items")) error("スキーマに配列要素がありません: " + path);
            auto items = current.at("items");
            if (items.is_array()) {
                bool ok; auto i = p.key.toULongLong(&ok);
                if (!ok || i >= items.size()) error("スキーマの配列範囲外: " + path);
                current = items.at(size_t(i));
            } else current = items;
        } else {
            if (!current.is_object() || !current.contains("properties") || !current["properties"].contains(s(p.key))) error("前段スキーマに変数がありません: " + path);
            current = Json(current.at("properties").at(s(p.key)));
        }
        return walk(current, root, index + 1);
    };
    return walk(schema, schema, 0);
}
QStringList Flow::validate() const {
    QStringList errors; int starts = 0, ends = 0;
    for (auto &[id, n] : nodes) { starts += n.kind == Kind::Start; ends += n.kind == Kind::End; }
    if (starts != 1 || ends != 1) errors << "始端・終端ノードはそれぞれ1つ必要です";
    std::map<std::pair<Id, unsigned>, unsigned> incoming;
    for (auto &e : edges) {
        if (!nodes.contains(e.from) || !nodes.contains(e.to)) { errors << "接続先ノードが存在しません"; continue; }
        if (e.output >= outputCount(nodes.at(e.from)) || e.input >= inputCount(nodes.at(e.to))) errors << "接続ポートが範囲外です";
        ++incoming[{e.to, e.input}];
    }
    for (size_t i = 0; i < edges.size(); ++i)
        for (size_t j = 0; j < i; ++j) if (edges[i] == edges[j]) errors << "同じ接続が重複しています";
    QSet<Id> reachable, terminal;
    std::function<void(Id, bool)> trace = [&](Id id, bool reverse) {
        auto &seen = reverse ? terminal : reachable;
        if (seen.contains(id)) return; seen.insert(id);
        for (auto &e : edges) if ((reverse ? e.to : e.from) == id) trace(reverse ? e.from : e.to, reverse);
    };
    for (auto &[id, n] : nodes) { if (n.kind == Kind::Start) trace(id, false); if (n.kind == Kind::End) trace(id, true); }
    for (auto &[id, n] : nodes) {
        const auto label = QString("[%1] ").arg(n.title.isEmpty() ? kindName(n.kind) : n.title);
        if (!reachable.contains(id)) errors << label + "始端から到達できません";
        if (!terminal.contains(id)) errors << label + "終端に到達できません";
        for (unsigned p = 0; p < inputCount(n); ++p) if (!incoming.contains({id, p})) errors << label + QString("入力%1が未接続です").arg(p);
        if (n.kind == Kind::And && n.inputs == 0) errors << label + "AND入力は1つ以上必要です";
        if (n.kind == Kind::Agent) {
            if (n.prompt.trimmed().isEmpty()) errors << label + "プロンプトが空です";
            if (n.agent != "Codex" && n.agent != "Claude Code" && n.agent != "OpenCode") errors << label + "不明なエージェントです";
            try { if (!n.schema.trimmed().isEmpty()) checkSchema(parseJson(n.schema)); }
            catch (const std::exception &e) { errors << label + "不正なJSON Schema: " + e.what(); }
        }
        try {
            std::vector<Json> inputs;
            for (auto &e : edges) if (e.to == id) { QSet<Id> seen; inputs.push_back(outputSchema(*this, e.from, seen)); }
            auto input = sources(inputs);
            if (n.kind == Kind::Agent) {
                QRegularExpression re(R"(\$\{([^}]+)\})"); auto it = re.globalMatch(n.prompt);
                while (it.hasNext()) schemaAt(input, it.next().captured(1));
            }
            if (n.kind == Kind::Branch) {
                for (auto &r : n.rules) schemaAt(input, r.variable);
                for (unsigned p = 0; p < outputCount(n); ++p) {
                    bool connected = false; for (auto &e : edges) if (e.from == id && e.output == p) connected = true;
                    if (!connected) errors << label + QString("分岐出力%1が未接続です").arg(p);
                }
            }
        } catch (const std::exception &e) { errors << label + QString::fromUtf8(e.what()); }
    }
    errors.removeDuplicates(); return errors;
}
}
