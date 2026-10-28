#include "strings.h"
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
    case State::Running: return strings::RunningValue; case State::Succeeded: return strings::SucceededValue;
    case State::Failed: return strings::Failed; case State::Cancelled: return strings::CancelledValue;
    default: throw std::runtime_error(strings::UnexecutedNodeHistory);
    }
}
State parseState(const QString &value) {
    if (value == strings::RunningValue) return State::Running;
    if (value == strings::SucceededValue) return State::Succeeded;
    if (value == strings::Failed) return State::Failed;
    if (value == strings::CancelledValue) return State::Cancelled;
    throw std::runtime_error(strings::UnknownHistoryState);
}
}
void saveNodeExecution(const QString &directory, const NodeExecution &execution) {
    const auto uuid = QUuid(execution.id);
    if (uuid.isNull() || !execution.startedAt.isValid()) throw std::runtime_error(strings::InvalidHistoryIdentity);
    if (!QDir().mkpath(directory)) throw std::runtime_error(strings::CreateHistoryDirectoryFailed);
    Json value = {{strings::Version, 2}, {strings::Id, text(execution.id)}, {strings::Node, execution.node}, {strings::Title, text(execution.title)},
        {strings::ExecutionNumber, execution.executionNumber}, {strings::Agent, text(execution.agent)}, {strings::Directory, text(execution.directory)},
        {strings::Model, text(execution.model)}, {strings::Effort, text(execution.effort)},
        {strings::StartedAt, text(execution.startedAt.toUTC().toString(Qt::ISODateWithMs))},
        {strings::FinishedAt, text(execution.finishedAt.toUTC().toString(Qt::ISODateWithMs))},
        {strings::State, text(stateKey(execution.state))}, {strings::Log, text(execution.log)}, {strings::Output, execution.output}, {strings::Sequence, execution.sequence}, {strings::StructuredOutput, execution.structuredOutput},
        {strings::RetainContext, execution.retainContext}, {strings::ContextResumed, execution.contextResumed}};
    QSaveFile file(QDir(directory).filePath(uuid.toString(QUuid::WithoutBraces) + strings::JsonFileExtension));
    const auto bytes = QByteArray::fromStdString(value.dump());
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error(text(strings::SaveHistoryFailed + file.errorString()));
}
std::vector<NodeExecution> loadNodeExecutions(const QString &directory, QStringList *errors) {
    std::vector<NodeExecution> result;
    for (const auto &name : QDir(directory).entryList({strings::JsonFilePattern}, QDir::Files)) {
        try {
            QFile file(QDir(directory).filePath(name)); if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(text(file.errorString()));
            const auto value = Json::parse(file.readAll().toStdString());
            if (value.at(strings::Version) == 1) continue;
            if (value.at(strings::Version) != 2) throw std::runtime_error(strings::UnsupportedHistoryVersion);
            NodeExecution execution; execution.id = string(value, strings::Id);
            if (QUuid(execution.id).isNull() || QUuid(execution.id).toString(QUuid::WithoutBraces) + strings::JsonFileExtension != name)
                throw std::runtime_error(strings::InvalidHistoryId);
            execution.node = value.at(strings::Node).get<Id>(); execution.title = string(value, strings::Title);
            execution.executionNumber = value.at(strings::ExecutionNumber).get<quint64>();
            execution.agent = string(value, strings::Agent); execution.directory = string(value, strings::Directory);
            execution.model = QString::fromStdString(value.value(strings::Model, std::string()));
            execution.effort = QString::fromStdString(value.value(strings::Effort, std::string()));
            execution.startedAt = QDateTime::fromString(string(value, strings::StartedAt), Qt::ISODateWithMs);
            execution.finishedAt = QDateTime::fromString(string(value, strings::FinishedAt), Qt::ISODateWithMs);
            if (!execution.startedAt.isValid()) throw std::runtime_error(strings::InvalidHistoryTime);
            execution.state = parseState(string(value, strings::State)); execution.log = string(value, strings::Log);
            execution.output = value.at(strings::Output); execution.sequence = value.value(strings::Sequence, quint64(0));
            execution.structuredOutput = value.value(strings::StructuredOutput, false);
            execution.retainContext = value.value(strings::RetainContext, false);
            execution.contextResumed = value.value(strings::ContextResumed, false);
            if (execution.state == State::Running) {
                execution.state = State::Cancelled;
                execution.log += strings::InterruptedHistoryNotice;
            }
            result.push_back(std::move(execution));
        } catch (const std::exception &error) {
            if (errors) errors->append(name + strings::ColonSpace + QString::fromUtf8(error.what()));
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
