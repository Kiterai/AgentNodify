#include "core/agents/definition.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <stdexcept>

namespace an {
QString prepareAgentLaunch(const AgentTask &task, ProcessJob &job, ProcessLaunch &launch) {
    const auto &definition=agentDefinition(task.node.agent);
    job.temp=std::make_unique<QTemporaryDir>();
    if (!job.temp->isValid()) throw std::runtime_error(strings::CreateTemporaryDirectoryFailed);
    launch.program=agentExecutable(definition,task.config);
    const auto prompt=definition.prepare(task,job,launch);
    if (definition.promptOnStdin) launch.standardInput=prompt.toUtf8();
    return prompt;
}
Json decodeAgentOutput(const QString &agent, const QByteArray &data, bool structured) {
    const auto &definition=agentDefinition(agent);
    AgentOutput output;
    for (const auto &line : data.split('\n')) {
        if(line.trimmed().isEmpty()) continue;
        const auto event=Json::parse(line.toStdString(),nullptr,false);
        if(event.is_discarded()) throw std::runtime_error(strings::InvalidCliJson);
        if(event.value(strings::IsError,false) || event.value(strings::Type,strings::Empty)==strings::ErrorValue) throw std::runtime_error(event.dump());
        definition.outputEvent(event,output);
    }
    if(structured && output.hasStructured) return output.structuredValue;
    if(!output.finalText.isEmpty()) output.text=output.finalText;
    if(output.text.isEmpty()) throw std::runtime_error(strings::MissingAgentResponse);
    return structured ? parseJson(output.text.trimmed()) : Json(output.text.toUtf8().toStdString());
}
QString decodeAgentSessionId(const QString &agent, const QByteArray &data) {
    const auto &definition=agentDefinition(agent);
    const QRegularExpression pattern(definition.sessionPattern);
    QString session;
    for (const auto &line : data.split('\n')) {
        const auto event=Json::parse(line.toStdString(),nullptr,false);
        if(!event.is_object()) continue;
        const auto *field=definition.sessionField(event);
        if(!field || !event.contains(field)) continue;
        if(!event.at(field).is_string()) throw std::runtime_error(strings::InvalidSessionId);
        const auto id=QString::fromStdString(event.at(field).get<std::string>());
        if(!pattern.match(id).hasMatch()) throw std::runtime_error(strings::InvalidSessionId);
        if(!session.isEmpty() && session!=id) throw std::runtime_error(strings::ConflictingSessionIds);
        session=id;
    }
    return session;
}
Json readAgentResponse(const QString &agent, const ProcessJob &job, bool structured) {
    if(!job.finalPath.isEmpty() && QFileInfo::exists(job.finalPath)) {
        QFile file(job.finalPath);
        if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error(strings::ReadFinalResponseFailed);
        const auto data=file.readAll();
        return structured ? Json::parse(data.toStdString()) : Json(data.toStdString());
    }
    return decodeAgentOutput(agent,job.stdoutData,structured);
}
}
