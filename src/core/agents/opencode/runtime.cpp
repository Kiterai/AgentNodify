#include "agent.h"
#include <stdexcept>

namespace an {
AgentDefinition openCodeAgent() {
    AgentDefinition d;
    d.name=strings::OpenCode; d.configKey=strings::OpenCodeExecutable;
#ifdef Q_OS_WIN
    d.defaultProgram=strings::OpenCodeWindowsExecutable;
    d.migrateProgram=[](const QString &program) {return program==strings::OpenCodeExecutable ? QString(strings::OpenCodeWindowsExecutable) : program;};
#else
    d.defaultProgram=strings::OpenCodeExecutable;
#endif
    d.legacyExecutable=&AgentConfig::opencode;
    d.fallbackEfforts={strings::Low,strings::Medium,strings::High,strings::Xhigh};
    d.promptOnStdin=false; d.sessionPattern=strings::OpenCodeSessionPattern; d.catalog=openCodeCatalog;
    d.progress=openCodeProgress;
    d.prepare=[](const AgentTask &task,ProcessJob &,ProcessLaunch &launch) {
        const auto &node=task.node; auto &args=launch.args; auto prompt=task.prompt;
        // A shared service can outlive cwd; keep tools in this project's process tree.
        args << strings::Run << strings::FormatArgument << strings::JsonFormat << strings::StandaloneArgument;
        if(!node.effort.isEmpty() && node.model.isEmpty()) throw std::runtime_error(strings::OpenCodeEffortRequiresModel);
        if(!node.model.isEmpty()) args << strings::ModelArgument << (node.effort.isEmpty() ? node.model : node.model.section('#',0,0)+strings::WholeInput+node.effort);
        if(!task.session.isEmpty()) args << strings::SessionArgument << task.session;
        if(!node.schema.trimmed().isEmpty()) prompt+=strings::JsonSchemaPromptSuffix+node.schema;
        args << strings::EndOfOptions << prompt;
        return prompt;
    };
    d.outputEvent=[](const Json &event,AgentOutput &output) {
        if(event.value(strings::Type,strings::Empty)==strings::TextFormat && event.contains(strings::Part)) output.text+=QString::fromStdString(event[strings::Part].value(strings::TextFormat,strings::Empty));
    };
    d.sessionField=[](const Json &event)->const char * {
        const auto type=event.value(strings::Type,Json());
        return type==strings::StepStart || type==strings::StepFinish || type==strings::TextFormat || type==strings::ToolUse ? strings::SessionID : nullptr;
    };
    return d;
}
}
