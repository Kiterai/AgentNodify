#pragma once
#include <nlohmann/json.hpp>
#include <QString>
#include <QPointF>
#include <map>
#include <vector>

namespace an {
using Json = nlohmann::json;
using Id = unsigned int;
enum class Kind { Start, End, Agent, And, Branch };
QString kindName(Kind kind);
QString kindKey(Kind kind);
Kind parseKind(const std::string &key);
struct Rule {
    QString variable;
    QString value;
    bool equals = true;
};
struct Node {
    Id id = 0;
    Kind kind = Kind::Agent;
    QString title;
    QPointF position;
    QString agent = "Codex";
    QString model;
    QString effort;
    QString prompt;
    QString schema;
    unsigned inputs = 2;
    std::vector<Rule> rules;
};
struct Edge {
    Id from, to;
    unsigned output = 0, input = 0;
    bool operator==(const Edge &) const = default;
};
struct Flow {
    quint64 nextNodeId = 1;
    QString name = QStringLiteral("新しいフロー");
    std::map<Id, Node> nodes;
    std::vector<Edge> edges;
    unsigned inputCount(const Node &n) const;
    unsigned outputCount(const Node &n) const;
    Json toJson() const;
    static Flow fromJson(const Json &json);
    static Flow initial();
    void save(const QString &path) const;
    static Flow load(const QString &path);
    QStringList validate() const;
};
Json parseJson(const QString &text);
QString display(const Json &value);
Json resolve(const Json &value, const QString &path);
QString interpolate(const QString &text, const Json &value);
void checkSchema(const Json &schema);
void validateOutput(const Json &schema, const Json &output);
Json schemaAt(const Json &schema, const QString &path);
}
