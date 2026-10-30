#include "agent.h"
#include "core/agents/common/progress.h"

namespace an {
namespace {
class OpenCodeProgress final : public AgentProgressProtocol {
public:
    QString fallbackKey(const Json &value) const override {
        if(!value.is_object() || !value.contains(strings::Part)) return {};
        auto id=agentText(value[strings::Part],strings::Id); if(id.isEmpty()) id=agentText(value[strings::Part],strings::PartID);
        return id.isEmpty() ? QString() : strings::SimplePart+id;
    }
    QString commandLabel(const QString &key,const QString &name) const override {
        return key.startsWith(strings::Opencode) && name==strings::Glob ? QString(strings::Glob) : QString();
    }
    bool summarizeFile(const QString &key,const QString &name) const override {return key.startsWith(strings::Opencode) && name==strings::Patch;}
    bool receive(const Json &value,AgentProgressSink &sink) override {
        const auto type=agentText(value,strings::Type);
        if(type==strings::StepStart || type==strings::StepFinish || type==strings::StepStartValue || type==strings::StepFinishValue) {sink.boundary();return true;}
        if((type!=strings::TextFormat && type!=strings::ToolUse) || !value.contains(strings::Part) || !value[strings::Part].is_object()) return false;
        const auto &part=value[strings::Part]; auto id=agentText(part,strings::Id);
        if(id.isEmpty()) id=agentText(part,strings::PartID); if(id.isEmpty()) id=QString::number(sink.lineNumber());
        const auto key=strings::Opencode+id;
        if(type==strings::TextFormat) {
            if(!part.contains(strings::TextFormat) || !part[strings::TextFormat].is_string()) {sink.simple(value);return true;}
            auto &entry=sink.item(key,ProgressKind::Message); entry.name=strings::AgentName; entry.content=agentText(part,strings::TextFormat).toStdString();return true;
        }
        if(!part.contains(strings::State) || !part[strings::State].is_object()) {sink.simple(value);return true;}
        const auto &state=part[strings::State]; const auto status=agentProgressStatus(agentText(state,strings::Status));
        const Json *input=state.contains(strings::Input) ? &state[strings::Input] : nullptr;
        const Json *output=state.contains(strings::ErrorValue) ? &state[strings::ErrorValue] : state.contains(strings::Output) ? &state[strings::Output] : state.contains(strings::Content) ? &state[strings::Content] : nullptr;
        if(!output && state.contains(strings::Metadata) && state[strings::Metadata].is_object() && state[strings::Metadata].contains(strings::Content)) output=&state[strings::Metadata][strings::Content];
        sink.tool(key,agentText(part,strings::Tool),status,input,output);
        if(state.contains(strings::Metadata) && state[strings::Metadata].is_object()) {
            const auto &outer=state[strings::Metadata];
            const auto &metadata=outer.contains(strings::Metadata) && outer[strings::Metadata].is_object() ? outer[strings::Metadata] : outer;
            if(metadata.contains(strings::Files) && metadata[strings::Files].is_array()) for(const auto &change : metadata[strings::Files]) {
                auto path=agentText(change,strings::FileValue); if(path.isEmpty()) path=agentText(change,strings::PathValue);
                sink.file(key,path,agentFileAccess(agentText(change,strings::Status)),status);
            }
        }
        sink.summarizeFileTool(key);return true;
    }
};
}
std::unique_ptr<AgentProgressProtocol> openCodeProgress() {return std::make_unique<OpenCodeProgress>();}
}
