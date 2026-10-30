#include "agent.h"
#include "core/agents/common/progress.h"
#include <QMap>

namespace an {
namespace {
class ClaudeProgress final : public AgentProgressProtocol {
    struct Block {QString key,input;bool tool=false,thinking=false;};
    QMap<int,Block> blocks_;
    QString message_;
    int block_=-1;
    void thinking(AgentProgressSink &sink,const QString &message,int index) {
        auto &entry=sink.item(strings::ClaudeThinking+message+strings::Colon+QString::number(index),ProgressKind::Thinking);
        entry.name=strings::ThinkingLabel;
    }
    void parse(const Json &value,AgentProgressSink &sink) {
        const auto type=agentText(value,strings::Type);
        if(type==strings::StreamEvent && value.contains(strings::Event) && value[strings::Event].is_object()) {
            const auto &event=value[strings::Event]; const auto kind=agentText(event,strings::Type);
            if(kind==strings::MessageStart && event.contains(strings::Message)) {message_=agentText(event[strings::Message],strings::Id);blocks_.clear();block_=-1;return;}
            const int index=event.value(strings::Index,0);
            if(kind==strings::ContentBlockStart && event.contains(strings::ContentBlock)) {
                block_=index;
                const auto &block=event[strings::ContentBlock]; const auto blockType=agentText(block,strings::Type);
                if(blockType==strings::ToolUse) {
                    const auto key=strings::ClaudeTool+agentText(block,strings::Id); blocks_[index]={key,{},true};
                    const Json input=block.value(strings::Input,Json::object());sink.tool(key,agentText(block,strings::Name),strings::RunningValue,&input,nullptr);
                } else if(blockType==strings::TextFormat) {
                    const auto key=strings::ClaudeText+message_+strings::Colon+QString::number(index);blocks_[index]={key,{},false};
                    auto &entry=sink.item(key,ProgressKind::Message);entry.name=strings::AgentName;entry.content=agentText(block,strings::TextFormat).toStdString();
                } else if(blockType==strings::Thinking || blockType==strings::RedactedThinking) {
                    thinking(sink,message_,index);blocks_[index]={strings::ClaudeThinking+message_+strings::Colon+QString::number(index),{},false,true};
                } else sink.simple(value,blockType);
                return;
            }
            if(kind==strings::ContentBlockDelta && event.contains(strings::Delta) &&
                (agentText(event[strings::Delta],strings::Type)==strings::ThinkingDelta || agentText(event[strings::Delta],strings::Type)==strings::SignatureDelta)) return;
            if(kind==strings::ContentBlockDelta && event.contains(strings::Delta) && blocks_.contains(index)) {
                block_=index;
                auto &block=blocks_[index];const auto &delta=event[strings::Delta];if(block.thinking) return;
                if(!block.tool && agentText(delta,strings::Type)==strings::TextDelta) {
                    auto &entry=sink.item(block.key,ProgressKind::Message);entry.content=(agentStringValue(entry.content)+agentText(delta,strings::TextFormat)).toStdString();
                } else if(block.tool && agentText(delta,strings::Type)==strings::InputJsonDelta) {
                    block.input+=agentText(delta,strings::PartialJson);const auto input=expandedAgentInput(Json(block.input.toStdString()));
                    const auto name=sink.item(block.key,ProgressKind::Tool).name;sink.tool(block.key,name,strings::RunningValue,&input,nullptr);
                } else sink.simple(value);
                return;
            }
            if(kind==strings::ContentBlockStop) {block_=-1;return;}
            if(kind==strings::MessageStop || kind==strings::MessageDelta) return;
            sink.simple(value);return;
        }
        if((type==strings::Assistant || type==strings::User) && value.contains(strings::Message) && value[strings::Message].is_object()) {
            const auto &message=value[strings::Message];
            if(!message.contains(strings::Content) || !message[strings::Content].is_array()) {sink.simple(value);return;}
            const auto id=agentText(message,strings::Id).isEmpty() ? QString::number(sink.lineNumber()) : agentText(message,strings::Id);
            int index=0;
            for(const auto &block : message[strings::Content]) {
                const auto kind=agentText(block,strings::Type);
                if(type==strings::Assistant && kind==strings::TextFormat) {
                    const auto text=agentText(block,strings::TextFormat);QString key;
                    const auto prefix=strings::ClaudeText+id+strings::Colon;
                    if(id==message_) {
                        const auto streamedIndex=message[strings::Content].size()==1 && block_>=0 ? block_ : index;
                        if(blocks_.contains(streamedIndex) && !blocks_[streamedIndex].tool) key=blocks_[streamedIndex].key;
                    }
                    if(key.isEmpty()) key=prefix+(agentText(value,strings::Uuid).isEmpty() ? QString::number(sink.lineNumber()) : agentText(value,strings::Uuid))+strings::Colon+QString::number(index);
                    auto &entry=sink.item(key,ProgressKind::Message);entry.name=strings::AgentName;entry.content=text.toStdString();
                } else if(type==strings::Assistant && (kind==strings::Thinking || kind==strings::RedactedThinking)) {
                    const int thinkingIndex=id==message_ && message[strings::Content].size()==1 && block_>=0 ? block_ : index;thinking(sink,id,thinkingIndex);
                } else if(kind==strings::ToolUse) {
                    const Json input=block.value(strings::Input,Json::object());sink.tool(strings::ClaudeTool+agentText(block,strings::Id),agentText(block,strings::Name),strings::RunningValue,&input,nullptr);
                } else if(kind==strings::ToolResult) {
                    const auto key=strings::ClaudeTool+agentText(block,strings::ToolUseId);const Json output=block.value(strings::Content,Json());
                    sink.tool(key,{},block.value(strings::IsError,false) ? strings::Failed : strings::SucceededValue,nullptr,&output);
                } else sink.simple(block,type+strings::Colon+kind);
                ++index;
            }
            return;
        }
        if(type==strings::Result && !value.value(strings::IsError,false)) {
            if(value.contains(strings::StructuredOutputValue)) sink.finalResponse(value[strings::StructuredOutputValue],true);
            else if(value.contains(strings::Result) && value[strings::Result].is_string() && !agentText(value,strings::Result).isEmpty()) sink.finalResponse(value[strings::Result]);
            else sink.simple(value);
            return;
        }
        sink.simple(value);
    }
public:
    bool receive(const Json &value,AgentProgressSink &sink) override {
        const auto type=agentText(value,strings::Type);
        if(type==strings::System && (agentText(value,strings::Subtype)==strings::ThinkingTokens || agentText(value,strings::Subtype)==strings::Init)) return true;
        if(type!=strings::Assistant && type!=strings::User && type!=strings::Result && type!=strings::StreamEvent) return false;
        parse(value,sink);return true;
    }
    QString fallbackKey(const Json &value) const override {
        if(agentText(value,strings::Type)!=strings::StreamEvent || !value.contains(strings::Event) || !value[strings::Event].is_object()) return {};
        const auto &event=value[strings::Event];
        const int index=event.contains(strings::Index) && event[strings::Index].is_number_integer() ? event[strings::Index].get<int>() : 0;
        return strings::SimpleStream+message_+strings::Colon+QString::number(index)+strings::Colon+agentText(event,strings::Type);
    }
    QString commandLabel(const QString &key,const QString &name) const override {
        return key.startsWith(strings::ClaudeTool) && name.compare(strings::PowerShell,Qt::CaseInsensitive)==0 ? QString(strings::PowerShell) : QString();
    }
    bool summarizeFile(const QString &key,const QString &name) const override {return key.startsWith(strings::ClaudeTool) && name.compare(strings::WriteValue,Qt::CaseInsensitive)==0;}
};
}
std::unique_ptr<AgentProgressProtocol> claudeCodeProgress() {return std::make_unique<ClaudeProgress>();}
}
