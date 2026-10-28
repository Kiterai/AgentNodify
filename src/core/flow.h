#pragma once
#include "shared/constants.h"
#include "shared/strings.h"
#include <nlohmann/json.hpp>
#include <QString>
#include <QStringList>
#include <QPointF>
#include <map>
#include <vector>

namespace an {
using Json = nlohmann::json;
using Id = unsigned int;
enum class Kind { Start, End, Agent, All, Branch, Script, Shell, ProjectCreate, FlowRun, FlowWait, Any, UserInput, Delay, Schedule };
bool isHostOperation(Kind kind);
bool hasNodeName(Kind kind);
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
    QString agent = strings::Codex;
    QString model;
    QString effort;
    QString prompt;
    QString schema;
    bool retainContext = false;
    unsigned inputs = defaults::InputPorts;
    std::vector<Rule> rules;
    QString scriptLanguage = strings::PythonLanguage;
    QString scriptPath, scriptDirectory;
    QString shellCommand, shellDirectory;
    QString shellOutput = strings::JsonFormat;
    QString targetFlow, projectPath;
    QStringList userChoices = {QString::fromUtf8(strings::Ok)};
    int waitDuration = defaults::WaitDuration;
    QString waitUnit = strings::Seconds;
    QString scheduleMonth = strings::Wildcard, scheduleDay = strings::Wildcard, scheduleHour = strings::Wildcard, scheduleMinute = strings::Wildcard;
    std::vector<int> scheduleWeekdays; // Qt weekday numbering: Monday=1, Sunday=7.
};
struct Edge {
    Id from, to;
    unsigned output = 0, input = 0;
    bool operator==(const Edge &) const = default;
};
struct Flow {
    quint64 nextNodeId = 1;
    QString name = QString::fromUtf8(strings::NewFlowName);
    std::map<Id, Node> nodes;
    std::vector<Edge> edges;
    QString sourcePath; // Runtime context only; not serialized into portable flows.
    unsigned inputCount(const Node &n) const;
    unsigned outputCount(const Node &n) const;
    Json toJson() const;
    static Flow fromJson(const Json &json);
    static Flow initial(bool withEnd = true);
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
