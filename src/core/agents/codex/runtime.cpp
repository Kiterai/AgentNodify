#include "agent.h"
#include <QFile>
#include <stdexcept>

namespace an {
AgentDefinition codexAgent() {
    AgentDefinition d;
    d.name=strings::Codex; d.configKey=strings::CodexExecutable; d.defaultProgram=strings::CodexExecutable;
    d.legacyExecutable=&AgentConfig::codex;
    d.fallbackEfforts={strings::Low,strings::Medium,strings::High,strings::Xhigh};
    d.sessionPattern=strings::UuidPattern; d.catalog=codexCatalog;
    d.progress=codexProgress;
    d.prepare=[](const AgentTask &task,ProcessJob &job,ProcessLaunch &launch) {
        const auto &node=task.node; auto &args=launch.args;
        job.finalPath=job.temp->filePath(strings::ResponseTxt);
        args << strings::Exec << strings::JsonArgument << strings::SkipGitCheckArgument << strings::LastMessageArgument << job.finalPath;
        args << strings::WorkingDirectoryArgument << task.directory << strings::SandboxArgument << strings::WorkspaceWrite;
        if(!node.schema.trimmed().isEmpty()) {
            const auto path=job.temp->filePath(strings::SchemaJson); QFile file(path);
            if(!file.open(QIODevice::WriteOnly) || file.write(node.schema.toUtf8())<0) throw std::runtime_error(strings::WriteSchemaFailed);
            args << strings::OutputSchemaArgument << path;
        }
        if(!node.model.isEmpty()) args << strings::ModelArgument << node.model;
        if(!node.effort.isEmpty()) args << strings::ConfigArgument << (strings::ReasoningEffortAssignment+QString::fromStdString(Json(node.effort.toStdString()).dump()));
        // Exec options precede resume; never use --last across parallel nodes.
        if(!task.session.isEmpty()) args << strings::Resume << task.session;
        args << strings::Dash;
        return task.prompt;
    };
    d.outputEvent=[](const Json &event,AgentOutput &output) {
        if(event.value(strings::Type,strings::Empty)==strings::ItemCompleted && event.contains(strings::Item) && event[strings::Item].value(strings::Type,strings::Empty)==strings::AgentMessage)
            output.finalText=QString::fromStdString(event[strings::Item].value(strings::TextFormat,strings::Empty));
    };
    d.sessionField=[](const Json &event)->const char * {return event.value(strings::Type,Json())==strings::ThreadStarted ? strings::ThreadId : nullptr;};
    return d;
}
}
