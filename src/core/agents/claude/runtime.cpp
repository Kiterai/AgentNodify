#include "agent.h"

namespace an {
AgentDefinition claudeCodeAgent() {
    AgentDefinition d;
    d.name=strings::ClaudeCode; d.configKey=strings::ClaudeExecutable; d.defaultProgram=strings::ClaudeExecutable;
    d.legacyExecutable=&AgentConfig::claude;
    d.fallbackEfforts={strings::Low,strings::Medium,strings::High,strings::Xhigh,strings::Max};
    d.sessionPattern=strings::UuidPattern; d.catalog=claudeCodeCatalog;
    d.progress=claudeCodeProgress;
    d.prepare=[](const AgentTask &task,ProcessJob &,ProcessLaunch &launch) {
        const auto &node=task.node; auto &args=launch.args;
        args << strings::PromptArgument << strings::VerboseArgument << strings::OutputFormatArgument << strings::StreamJson << strings::PartialMessagesArgument;
        args << strings::PermissionModeArgument << strings::AcceptEdits;
        if(!node.effort.isEmpty()) args << strings::EffortArgument << node.effort;
        if(!node.model.isEmpty()) args << strings::ModelArgument << node.model;
        if(!node.schema.trimmed().isEmpty()) args << strings::JsonSchemaArgument << node.schema;
        if(!task.session.isEmpty()) args << strings::ResumeArgument << task.session;
        return task.prompt;
    };
    d.outputEvent=[](const Json &event,AgentOutput &output) {
        if(event.value(strings::Type,strings::Empty)==strings::Result) {
            if(event.contains(strings::StructuredOutputValue)) {output.structuredValue=event[strings::StructuredOutputValue];output.hasStructured=true;}
            if(event.contains(strings::Result) && event[strings::Result].is_string()) output.finalText=QString::fromStdString(event[strings::Result].get<std::string>());
        } else if(event.value(strings::Type,strings::Empty)==strings::Assistant && event.contains(strings::Message)) {
            for(const auto &block : event[strings::Message].value(strings::Content,Json::array()))
                if(block.value(strings::Type,strings::Empty)==strings::TextFormat) output.text+=QString::fromStdString(block.value(strings::TextFormat,strings::Empty));
        }
    };
    d.sessionField=[](const Json &event)->const char * {
        const auto type=event.value(strings::Type,Json());
        return type==strings::Result || (type==strings::System && event.value(strings::Subtype,Json())==strings::Init) ? strings::SessionId : nullptr;
    };
    return d;
}
}
