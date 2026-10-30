#include "agent.h"
#include "core/agents/common/progress.h"

namespace an {
namespace {
class CodexProgress final : public AgentProgressProtocol {
public:
    QString fallbackKey(const Json &value) const override {
        return value.is_object() && value.contains(strings::Item) && !agentText(value[strings::Item],strings::Id).isEmpty()
            ? strings::SimpleItem+agentText(value[strings::Item],strings::Id) : QString();
    }
    QString commandLabel(const QString &key,const QString &name) const override {
        return key.startsWith(strings::CodexValue) && name==strings::CommandExecution ? QString(strings::ShellName) : QString();
    }
    bool receive(const Json &value,AgentProgressSink &sink) override {
        const auto type=agentText(value,strings::Type);
        if(type==strings::TurnStarted || type==strings::TurnStart || type==strings::TurnCompleted) {sink.boundary();return true;}
        if(type==strings::RateLimitEvent || type==strings::ThreadStarted) return true;
        if(!type.startsWith(strings::ItemValue) || !value.contains(strings::Item) || !value[strings::Item].is_object()) return false;
        const auto &data=value[strings::Item]; const auto kind=agentText(data,strings::Type);
        auto id=agentText(data,strings::Id); if(id.isEmpty()) id=QString::number(sink.lineNumber());
        const auto key=strings::CodexValue+id;
        auto status=agentProgressStatus(agentText(data,strings::Status));
        if(agentText(data,strings::Status).isEmpty() && type==strings::ItemCompleted) status=strings::SucceededValue;
        if(kind==strings::AgentMessage) {
            if(!data.contains(strings::TextFormat) || !data[strings::TextFormat].is_string()) {sink.simple(value);return true;}
            auto &entry=sink.item(key,ProgressKind::Message); entry.name=strings::AgentName; entry.content=agentText(data,strings::TextFormat).toStdString(); return true;
        }
        if(kind==strings::CommandExecution) {
            if(data.contains(strings::ExitCode) && data[strings::ExitCode].is_number_integer() && data[strings::ExitCode]!=0) status=strings::Failed;
            const Json input={{strings::Command,data.value(strings::Command,Json())}};
            const Json *output=data.contains(strings::AggregatedOutput) ? &data[strings::AggregatedOutput] : nullptr;
            sink.tool(key,strings::CommandExecution,status,&input,output); return true;
        }
        if(kind==strings::McpToolCall) {
            const auto name=agentText(data,strings::Server)+strings::Slash+agentText(data,strings::Tool);
            const Json *input=data.contains(strings::Arguments) ? &data[strings::Arguments] : nullptr;
            const Json *output=data.contains(strings::ErrorValue) && !data[strings::ErrorValue].is_null() ? &data[strings::ErrorValue] : data.contains(strings::Result) ? &data[strings::Result] : nullptr;
            if(data.contains(strings::ErrorValue) && !data[strings::ErrorValue].is_null()) status=strings::Failed;
            sink.tool(key,name,status,input,output);return true;
        }
        if(kind==strings::WebSearch) {
            const Json input={{strings::Query,data.value(strings::Query,Json())}};
            const Json *output=data.contains(strings::Result) ? &data[strings::Result] : nullptr;
            sink.tool(key,strings::WebSearch,status,&input,output);return true;
        }
        if(kind==strings::FileChange && data.contains(strings::Changes) && data[strings::Changes].is_array()) {
            bool found=false;
            for(const auto &change : data[strings::Changes]) if(!agentText(change,strings::PathValue).isEmpty()) {
                sink.file(key,agentText(change,strings::PathValue),agentFileAccess(agentText(change,strings::Kind)),status); found=true;
            }
            if(!found) sink.simple(value,kind);
            return true;
        }
        sink.simple(value,kind);return true;
    }
};
}
std::unique_ptr<AgentProgressProtocol> codexProgress() {return std::make_unique<CodexProgress>();}
}
