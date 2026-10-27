#include "history.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace an {
namespace {
std::string text(const QString &value) { return value.toUtf8().toStdString(); }
QString string(const Json &value, const char *key) { return QString::fromStdString(value.at(key).get<std::string>()); }
QString stateKey(State state) {
    switch (state) {
    case State::Running: return "running"; case State::Succeeded: return "succeeded";
    case State::Failed: return "failed"; case State::Cancelled: return "cancelled";
    default: throw std::runtime_error("実行されていないノードは履歴に保存できません");
    }
}
State parseState(const QString &value) {
    if (value == "running") return State::Running;
    if (value == "succeeded") return State::Succeeded;
    if (value == "failed") return State::Failed;
    if (value == "cancelled") return State::Cancelled;
    throw std::runtime_error("不明な実行履歴の状態です");
}
}
void saveNodeExecution(const QString &directory, const NodeExecution &execution) {
    const auto uuid = QUuid(execution.id);
    if (uuid.isNull() || !execution.startedAt.isValid()) throw std::runtime_error("実行履歴のIDまたは日時が不正です");
    if (!QDir().mkpath(directory)) throw std::runtime_error("実行履歴の保存先を作成できません");
    Json value = {{"version", 2}, {"id", text(execution.id)}, {"node", execution.node}, {"title", text(execution.title)},
        {"executionNumber", execution.executionNumber}, {"agent", text(execution.agent)}, {"directory", text(execution.directory)},
        {"model", text(execution.model)}, {"effort", text(execution.effort)},
        {"startedAt", text(execution.startedAt.toUTC().toString(Qt::ISODateWithMs))},
        {"finishedAt", text(execution.finishedAt.toUTC().toString(Qt::ISODateWithMs))},
        {"state", text(stateKey(execution.state))}, {"log", text(execution.log)}, {"output", execution.output}, {"sequence", execution.sequence}, {"structuredOutput", execution.structuredOutput},
        {"retainContext", execution.retainContext}, {"contextResumed", execution.contextResumed}};
    QSaveFile file(QDir(directory).filePath(uuid.toString(QUuid::WithoutBraces) + ".json"));
    const auto bytes = QByteArray::fromStdString(value.dump());
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error(text("実行履歴を保存できません: " + file.errorString()));
}
std::vector<NodeExecution> loadNodeExecutions(const QString &directory, QStringList *errors) {
    std::vector<NodeExecution> result;
    for (const auto &name : QDir(directory).entryList({"*.json"}, QDir::Files)) {
        try {
            QFile file(QDir(directory).filePath(name)); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(text(file.errorString()));
            const auto value = Json::parse(file.readAll().toStdString());
            if (value.at("version") == 1) continue;
            if (value.at("version") != 2) throw std::runtime_error("未対応の実行履歴の形式です");
            NodeExecution execution; execution.id = string(value, "id");
            if (QUuid(execution.id).isNull() || QUuid(execution.id).toString(QUuid::WithoutBraces) + ".json" != name)
                throw std::runtime_error("実行履歴のIDが不正です");
            execution.node = value.at("node").get<Id>(); execution.title = string(value, "title");
            execution.executionNumber = value.at("executionNumber").get<quint64>();
            execution.agent = string(value, "agent"); execution.directory = string(value, "directory");
            execution.model = QString::fromStdString(value.value("model", std::string()));
            execution.effort = QString::fromStdString(value.value("effort", std::string()));
            execution.startedAt = QDateTime::fromString(string(value, "startedAt"), Qt::ISODateWithMs);
            execution.finishedAt = QDateTime::fromString(string(value, "finishedAt"), Qt::ISODateWithMs);
            if (!execution.startedAt.isValid()) throw std::runtime_error("実行履歴の日時が不正です");
            execution.state = parseState(string(value, "state")); execution.log = string(value, "log");
            execution.output = value.at("output"); execution.sequence = value.value("sequence", quint64(0));
            execution.structuredOutput = value.value("structuredOutput", false);
            execution.retainContext = value.value("retainContext", false);
            execution.contextResumed = value.value("contextResumed", false);
            if (execution.state == State::Running) {
                execution.state = State::Cancelled;
                execution.log += "\n[前回の実行は終了を確認できなかったため、中断として表示します]\n";
            }
            result.push_back(std::move(execution));
        } catch (const std::exception &error) {
            if (errors) errors->append(name + ": " + QString::fromUtf8(error.what()));
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        if (a.sequence != b.sequence) return a.sequence < b.sequence;
        if (a.startedAt != b.startedAt) return a.startedAt < b.startedAt;
        return a.id < b.id;
    });
    return result;
}
}
