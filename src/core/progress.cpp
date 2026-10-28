#include "shared/constants.h"
#include "shared/strings.h"
#include "progress.h"
#include <QDir>
#include <algorithm>
#include <stdexcept>

namespace an {
namespace {
QString str(const Json &j, const char *key) {
    if (!j.is_object() || !j.contains(key) || !j.at(key).is_string()) return {};
    return QString::fromStdString(j.at(key).get<std::string>());
}
QString stringValue(const Json &j) { return j.is_string() ? QString::fromStdString(j.get<std::string>()) : QString(); }
QString statusOf(const QString &status) {
    if (status == strings::Completed || status == strings::Success || status == strings::SucceededValue) return strings::SucceededValue;
    if (status == strings::Failed || status == strings::ErrorValue) return strings::Failed;
    if (status == strings::CancelledValue || status == strings::Canceled) return strings::CancelledValue;
    return strings::RunningValue;
}
QString accessOf(const QString &kind) {
    if (kind == strings::Add || kind == strings::Added || kind == strings::Create) return strings::FileCreatedLabel;
    if (kind == strings::Delete || kind == strings::Deleted || kind == strings::Remove) return strings::FileDeletedLabel;
    if (kind == strings::Rename || kind == strings::Renamed || kind == strings::Move) return strings::FileMovedLabel;
    return strings::FileChangedLabel;
}
Json expandedInput(const Json &value) {
    if (!value.is_string()) return value;
    auto parsed = displayJson(stringValue(value)); return parsed.is_discarded() ? value : parsed;
}
bool equivalent(const Json &a, const Json &b) {
    return a == b || expandedInput(a) == expandedInput(b) || (a.is_string() && b.is_string() && stringValue(a).trimmed() == stringValue(b).trimmed());
}
}
Json displayJson(const QString &text) {
    try {
        return Json::parse(text.toUtf8().toStdString(), [](int depth, Json::parse_event_t, Json &) {
            if (depth > limits::JsonComparisonDepth) throw std::runtime_error(strings::DisplayDepthLimit); return true;
        }, false);
    } catch (...) { return Json(Json::value_t::discarded); }
}
QString progressStatus(const QString &status) {
    if (status == strings::SucceededValue) return strings::Succeeded;
    if (status == strings::Failed) return strings::FailedLabel;
    if (status == strings::CancelledValue) return strings::Cancelled;
    if (status == strings::Unknown) return strings::UnknownResultLabel;
    return strings::Running;
}
ProgressItem &ProgressParser::item(const QString &key, ProgressKind kind) {
    auto it = indexes_.find(key);
    if (it != indexes_.end()) return items_[it.value()];
    indexes_[key] = items_.size(); items_.push_back({}); auto &result = items_.back(); result.key = key; result.kind = kind;
    result.spaceBefore = pendingBoundary_; pendingBoundary_ = false; return result;
}
void ProgressParser::append(const QString &chunk) {
    pending_ += chunk;
    qsizetype start = 0, end;
    while ((end = pending_.indexOf('\n', start)) >= 0) { line(pending_.mid(start, end - start)); start = end + 1; }
    pending_.remove(0, start);
}
void ProgressParser::finish(State state) {
    if (!pending_.isEmpty()) { const auto rest = std::move(pending_); pending_.clear(); line(rest); }
    for (auto &entry : items_) if ((entry.kind == ProgressKind::Tool || entry.kind == ProgressKind::FileAccess) && entry.status == strings::RunningValue)
        entry.status = state == State::Cancelled ? strings::CancelledValue : state == State::Failed ? strings::Failed : strings::Unknown;
}
void ProgressParser::finalResponse(const Json &output, bool structured) {
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) if (it->kind == ProgressKind::Message && equivalent(it->content, output)) {
        const bool quotedJson = it->content.is_string() && displayJson(stringValue(it->content)).is_string();
        it->jsonContent = it->jsonContent || structured || quotedJson;
        it->name = strings::FinalResponseLabel; it->finalResponse = true; it->content = output; return;
    }
    auto &entry = item(strings::FinalResponse, ProgressKind::Message); entry.name = strings::FinalResponseLabel; entry.finalResponse = true; entry.content = output; entry.jsonContent = structured;
}
void ProgressParser::line(const QString &text) {
    ++line_; if (text.trimmed().isEmpty()) return;
    const auto value = plainLog_ ? Json(Json::value_t::discarded) : displayJson(text);
    if (line_ == 1 && value.is_object() && str(value, strings::Type) == strings::AgentnodifyPrompt && value.contains(strings::Prompt) && value[strings::Prompt].is_string()) {
        auto &entry = item(strings::InputPrompt, ProgressKind::Prompt); entry.name = strings::InputPromptLabel; entry.text = str(value, strings::Prompt); return;
    }
    if (value.is_discarded() || !value.is_object()) {
        auto &entry = item(QString(strings::PlainEventKey).arg(line_), ProgressKind::Simple); entry.text = text; return;
    }
    try { event(value); } catch (const std::exception &) { simple(value, strings::UnrecognizedEvent); }
}
void ProgressParser::simple(const Json &value, const QString &label) {
    QString key;
    if (value.is_object() && value.contains(strings::Item) && !str(value[strings::Item], strings::Id).isEmpty()) key = strings::SimpleItem + str(value[strings::Item], strings::Id);
    else if (value.is_object() && value.contains(strings::Part)) {
        auto id = str(value[strings::Part], strings::Id); if (id.isEmpty()) id = str(value[strings::Part], strings::PartID);
        if (!id.isEmpty()) key = strings::SimplePart + id;
    }
    if (str(value, strings::Type) == strings::StreamEvent && value.contains(strings::Event) && value[strings::Event].is_object()) {
        const auto &event = value[strings::Event];
        const int index = event.contains(strings::Index) && event[strings::Index].is_number_integer() ? event[strings::Index].get<int>() : 0;
        key = strings::SimpleStream + claudeMessage_ + strings::Colon + QString::number(index) + strings::Colon + str(event, strings::Type);
    }
    if (key.isEmpty()) key = QString(strings::EventKey).arg(line_).arg(items_.size());
    auto &entry = item(key, ProgressKind::Simple);
    entry.name = label.isEmpty() ? str(value, strings::Type) : label;
    if (entry.name.isEmpty()) entry.name = strings::EventLabel;
    entry.text = str(value, strings::Subtype); if (entry.text.isEmpty()) entry.text = str(value, strings::Message);
    entry.content = value;
}
void ProgressParser::file(const QString &key, const QString &path, const QString &access, const QString &status) {
    if (path.isEmpty() || path.contains(strings::UrlSchemeSeparator)) return; // A remote resource is not a filesystem access.
    auto normalized = path; normalized.replace('\\', '/');
    if (!directory_.isEmpty() && QDir::isRelativePath(normalized)) normalized = QDir(directory_).filePath(normalized);
    normalized = QDir::cleanPath(normalized);
    auto &entry = item(key + strings::FileEventSeparator + access + strings::Colon + normalized, ProgressKind::FileAccess);
    entry.path = normalized; entry.name = access;
    if (entry.status.isEmpty() || entry.status == strings::RunningValue || status != strings::RunningValue) entry.status = status;
}
void ProgressParser::files(const QString &key, const QString &name, const Json &raw, const QString &status) {
    const auto input = expandedInput(raw); if (!input.is_object()) return;
    const auto toolName = name.toLower().section('/', -1).section('.', -1).section(strings::ToolNamespaceSeparator, -1);
    QString access;
    if (toolName == strings::Read || toolName == strings::ReadFile) access = strings::FileReadLabel;
    else if (toolName == strings::Write || toolName == strings::WriteFile) access = strings::FileWriteLabel;
    else if (toolName == strings::Edit || toolName == strings::Multiedit || toolName == strings::Notebookedit) access = strings::FileChangedLabel;
    // A generic MCP read/write may target a database or remote resource. Require
    // a file-specific tool/argument before attributing it to the filesystem.
    if ((name.contains('/') || name.contains(strings::ToolNamespaceSeparator)) && !toolName.endsWith(strings::File) &&
        !input.contains(strings::FilePath) && !input.contains(strings::FilePathValue) && !input.contains(strings::NotebookPath)) access.clear();
    if (!access.isEmpty()) {
        auto path = str(input, strings::FilePath); if (path.isEmpty()) path = str(input, strings::FilePathValue);
        if (path.isEmpty()) path = str(input, strings::PathValue); if (path.isEmpty()) path = str(input, strings::NotebookPath);
        file(key, path, access, status);
    }
    if (toolName == strings::Patch || toolName == strings::ApplyPatch) {
        auto patch = str(input, strings::PatchText); if (patch.isEmpty()) patch = str(input, strings::Patch);
        for (const auto &line : patch.split('\n')) {
            if (line.startsWith(strings::PatchAddPrefix)) file(key, line.mid(sizeof(strings::PatchAddPrefix) - 1).trimmed(), strings::FileCreatedLabel, status);
            else if (line.startsWith(strings::PatchDeletePrefix)) file(key, line.mid(sizeof(strings::PatchDeletePrefix) - 1).trimmed(), strings::FileDeletedLabel, status);
            else if (line.startsWith(strings::PatchUpdatePrefix)) file(key, line.mid(sizeof(strings::PatchUpdatePrefix) - 1).trimmed(), strings::FileChangedLabel, status);
            else if (line.startsWith(strings::PatchMovePrefix) || line.startsWith(strings::PatchMoveAlternativePrefix)) file(key, line.mid(sizeof(strings::PatchMovePrefix) - 1).trimmed(), strings::FileMovedLabel, status);
        }
    }
}
void ProgressParser::tool(const QString &key, const QString &name, const QString &status, const Json *input, const Json *output) {
    auto &entry = item(key, ProgressKind::Tool);
    if (!name.isEmpty()) entry.name = name;
    if (entry.name.isEmpty()) entry.name = strings::ToolLabel;
    if (entry.status.isEmpty() || entry.status == strings::RunningValue || status != strings::RunningValue) entry.status = status;
    if (input) { entry.input = *input; entry.hasInput = true; }
    if (output) { entry.output = *output; entry.hasOutput = true; }
    if (key.startsWith(strings::CodexValue) && entry.name == strings::CommandExecution) entry.commandLabel = strings::ShellName;
    else if (key.startsWith(strings::ClaudeTool) && entry.name.compare(strings::PowerShell, Qt::CaseInsensitive) == 0) entry.commandLabel = strings::PowerShell;
    else if (key.startsWith(strings::Opencode) && entry.name == strings::Glob) entry.commandLabel = strings::Glob;
    if (!entry.commandLabel.isEmpty()) entry.command = str(expandedInput(entry.input), entry.commandLabel == strings::Glob ? strings::Pattern : strings::Command);
    // Copy before adding file rows, which may reallocate items_.
    const auto toolName = entry.name; const auto arguments = entry.input; const auto effectiveStatus = entry.status;
    files(key, toolName, arguments, effectiveStatus);
    summarizeFileTool(key);
}
void ProgressParser::summarizeFileTool(const QString &key) {
    auto &entry = item(key, ProgressKind::Tool);
    if (!((key.startsWith(strings::Opencode) && entry.name == strings::Patch) ||
        (key.startsWith(strings::ClaudeTool) && entry.name.compare(strings::WriteValue, Qt::CaseInsensitive) == 0))) return;
    for (auto &file : items_) if (file.kind == ProgressKind::FileAccess && file.key.startsWith(key + strings::FileEventSeparator)) {
        entry.hidden = true;
        if (entry.spaceBefore) { file.spaceBefore = true; entry.spaceBefore = false; }
        return;
    }
}
void ProgressParser::event(const Json &value) {
    const auto type = str(value, strings::Type);
    if (type == strings::TurnStarted || type == strings::TurnStart || type == strings::TurnCompleted ||
        type == strings::StepStart || type == strings::StepFinish || type == strings::StepStartValue || type == strings::StepFinishValue) {
        pendingBoundary_ = !items_.empty(); return;
    }
    if (type == strings::RateLimitEvent || type == strings::ThreadStarted ||
        (type == strings::System && (str(value, strings::Subtype) == strings::ThinkingTokens || str(value, strings::Subtype) == strings::Init))) return;
    if (type == strings::Assistant || type == strings::User || type == strings::Result || type == strings::StreamEvent) { claude(value); return; }
    if (type.startsWith(strings::ItemValue) && value.contains(strings::Item) && value[strings::Item].is_object()) {
        const auto &data = value[strings::Item]; const auto kind = str(data, strings::Type);
        auto id = str(data, strings::Id); if (id.isEmpty()) id = QString::number(line_);
        const auto key = strings::CodexValue + id;
        auto status = statusOf(str(data, strings::Status));
        if (str(data, strings::Status).isEmpty() && type == strings::ItemCompleted) status = strings::SucceededValue;
        if (kind == strings::AgentMessage) {
            if (!data.contains(strings::TextFormat) || !data[strings::TextFormat].is_string()) { simple(value); return; }
            auto &entry = item(key, ProgressKind::Message); entry.name = strings::AgentName; entry.content = str(data, strings::TextFormat).toStdString(); return;
        }
        if (kind == strings::CommandExecution) {
            if (data.contains(strings::ExitCode) && data[strings::ExitCode].is_number_integer() && data[strings::ExitCode] != 0) status = strings::Failed;
            const Json input = {{strings::Command, data.value(strings::Command, Json())}};
            const Json *output = data.contains(strings::AggregatedOutput) ? &data[strings::AggregatedOutput] : nullptr;
            tool(key, strings::CommandExecution, status, &input, output); return;
        }
        if (kind == strings::McpToolCall) {
            const auto name = str(data, strings::Server) + strings::Slash + str(data, strings::Tool);
            const Json *input = data.contains(strings::Arguments) ? &data[strings::Arguments] : nullptr;
            const Json *output = data.contains(strings::ErrorValue) && !data[strings::ErrorValue].is_null() ? &data[strings::ErrorValue] : data.contains(strings::Result) ? &data[strings::Result] : nullptr;
            if (data.contains(strings::ErrorValue) && !data[strings::ErrorValue].is_null()) status = strings::Failed;
            tool(key, name, status, input, output); return;
        }
        if (kind == strings::WebSearch) {
            const Json input = {{strings::Query, data.value(strings::Query, Json())}};
            const Json *output = data.contains(strings::Result) ? &data[strings::Result] : nullptr;
            tool(key, strings::WebSearch, status, &input, output); return;
        }
        if (kind == strings::FileChange && data.contains(strings::Changes) && data[strings::Changes].is_array()) {
            bool found = false;
            for (const auto &change : data[strings::Changes]) if (!str(change, strings::PathValue).isEmpty()) { file(key, str(change, strings::PathValue), accessOf(str(change, strings::Kind)), status); found = true; }
            if (!found) simple(value, kind);
            return;
        }
        simple(value, kind); return;
    }
    if ((type == strings::TextFormat || type == strings::ToolUse) && value.contains(strings::Part) && value[strings::Part].is_object()) {
        const auto &part = value[strings::Part]; auto id = str(part, strings::Id);
        if (id.isEmpty()) id = str(part, strings::PartID); if (id.isEmpty()) id = QString::number(line_);
        const auto key = strings::Opencode + id;
        if (type == strings::TextFormat) {
            if (!part.contains(strings::TextFormat) || !part[strings::TextFormat].is_string()) { simple(value); return; }
            auto &entry = item(key, ProgressKind::Message); entry.name = strings::AgentName; entry.content = str(part, strings::TextFormat).toStdString(); return;
        }
        if (!part.contains(strings::State) || !part[strings::State].is_object()) { simple(value); return; }
        const auto &state = part[strings::State]; const auto status = statusOf(str(state, strings::Status));
        const Json *input = state.contains(strings::Input) ? &state[strings::Input] : nullptr;
        const Json *output = state.contains(strings::ErrorValue) ? &state[strings::ErrorValue] : state.contains(strings::Output) ? &state[strings::Output] : state.contains(strings::Content) ? &state[strings::Content] : nullptr;
        if (!output && state.contains(strings::Metadata) && state[strings::Metadata].is_object() && state[strings::Metadata].contains(strings::Content)) output = &state[strings::Metadata][strings::Content];
        tool(key, str(part, strings::Tool), status, input, output);
        // V2 CLI diagnostics can nest tool metadata one level deeper.
        if (state.contains(strings::Metadata) && state[strings::Metadata].is_object()) {
            const auto &outer = state[strings::Metadata];
            const auto &metadata = outer.contains(strings::Metadata) && outer[strings::Metadata].is_object() ? outer[strings::Metadata] : outer;
            if (metadata.contains(strings::Files) && metadata[strings::Files].is_array()) for (const auto &change : metadata[strings::Files]) {
                auto path = str(change, strings::FileValue); if (path.isEmpty()) path = str(change, strings::PathValue);
                file(key, path, accessOf(str(change, strings::Status)), status);
            }
        }
        summarizeFileTool(key);
        return;
    }
    simple(value);
}
void ProgressParser::thinking(const QString &message, int index) {
    auto &entry = item(strings::ClaudeThinking + message + strings::Colon + QString::number(index), ProgressKind::Thinking);
    entry.name = strings::ThinkingLabel;
}
void ProgressParser::claude(const Json &value) {
    const auto type = str(value, strings::Type);
    if (type == strings::StreamEvent && value.contains(strings::Event) && value[strings::Event].is_object()) {
        const auto &event = value[strings::Event]; const auto kind = str(event, strings::Type);
        if (kind == strings::MessageStart && event.contains(strings::Message)) { claudeMessage_ = str(event[strings::Message], strings::Id); blocks_.clear(); claudeBlock_ = -1; return; }
        const int index = event.value(strings::Index, 0);
        if (kind == strings::ContentBlockStart && event.contains(strings::ContentBlock)) {
            claudeBlock_ = index;
            const auto &block = event[strings::ContentBlock]; const auto blockType = str(block, strings::Type);
            if (blockType == strings::ToolUse) {
                const auto key = strings::ClaudeTool + str(block, strings::Id); blocks_[index] = {key, {}, true};
                const Json input = block.value(strings::Input, Json::object()); tool(key, str(block, strings::Name), strings::RunningValue, &input, nullptr);
            } else if (blockType == strings::TextFormat) {
                const auto key = strings::ClaudeText + claudeMessage_ + strings::Colon + QString::number(index); blocks_[index] = {key, {}, false};
                auto &entry = item(key, ProgressKind::Message); entry.name = strings::AgentName; entry.content = str(block, strings::TextFormat).toStdString();
            } else if (blockType == strings::Thinking || blockType == strings::RedactedThinking) {
                thinking(claudeMessage_, index);
                blocks_[index] = {strings::ClaudeThinking + claudeMessage_ + strings::Colon + QString::number(index), {}, false, true};
            } else simple(value, blockType);
            return;
        }
        if (kind == strings::ContentBlockDelta && event.contains(strings::Delta) &&
            (str(event[strings::Delta], strings::Type) == strings::ThinkingDelta || str(event[strings::Delta], strings::Type) == strings::SignatureDelta)) return;
        if (kind == strings::ContentBlockDelta && event.contains(strings::Delta) && blocks_.contains(index)) {
            claudeBlock_ = index;
            auto &block = blocks_[index]; const auto &delta = event[strings::Delta];
            if (block.thinking) return;
            if (!block.tool && str(delta, strings::Type) == strings::TextDelta) {
                auto &entry = item(block.key, ProgressKind::Message); entry.content = (stringValue(entry.content) + str(delta, strings::TextFormat)).toStdString();
            } else if (block.tool && str(delta, strings::Type) == strings::InputJsonDelta) {
                block.input += str(delta, strings::PartialJson); const auto input = expandedInput(Json(block.input.toStdString()));
                const auto name = item(block.key, ProgressKind::Tool).name; tool(block.key, name, strings::RunningValue, &input, nullptr);
            } else simple(value);
            return;
        }
        if (kind == strings::ContentBlockStop) { claudeBlock_ = -1; return; }
        if (kind == strings::MessageStop || kind == strings::MessageDelta) return;
        simple(value); return;
    }
    if ((type == strings::Assistant || type == strings::User) && value.contains(strings::Message) && value[strings::Message].is_object()) {
        const auto &message = value[strings::Message];
        if (!message.contains(strings::Content) || !message[strings::Content].is_array()) { simple(value); return; }
        const auto messageId = str(message, strings::Id).isEmpty() ? QString::number(line_) : str(message, strings::Id);
        int index = 0;
        for (const auto &block : message[strings::Content]) {
            const auto kind = str(block, strings::Type);
            if (type == strings::Assistant && kind == strings::TextFormat) {
                const auto text = str(block, strings::TextFormat); QString key;
                const auto prefix = strings::ClaudeText + messageId + strings::Colon;
                if (messageId == claudeMessage_) {
                    const auto streamedIndex = message[strings::Content].size() == 1 && claudeBlock_ >= 0 ? claudeBlock_ : index;
                    if (blocks_.contains(streamedIndex) && !blocks_[streamedIndex].tool) key = blocks_[streamedIndex].key;
                }
                // Without partial events, UUID identifies a complete message. Different
                // complete blocks may intentionally contain exactly the same text.
                if (key.isEmpty()) key = prefix + (str(value, strings::Uuid).isEmpty() ? QString::number(line_) : str(value, strings::Uuid)) + strings::Colon + QString::number(index);
                auto &entry = item(key, ProgressKind::Message); entry.name = strings::AgentName; entry.content = text.toStdString();
            } else if (type == strings::Assistant && (kind == strings::Thinking || kind == strings::RedactedThinking)) {
                const int thinkingIndex = messageId == claudeMessage_ && message[strings::Content].size() == 1 && claudeBlock_ >= 0 ? claudeBlock_ : index;
                thinking(messageId, thinkingIndex);
            } else if (kind == strings::ToolUse) {
                const Json input = block.value(strings::Input, Json::object());
                tool(strings::ClaudeTool + str(block, strings::Id), str(block, strings::Name), strings::RunningValue, &input, nullptr);
            } else if (kind == strings::ToolResult) {
                const auto key = strings::ClaudeTool + str(block, strings::ToolUseId); const Json output = block.value(strings::Content, Json());
                tool(key, {}, block.value(strings::IsError, false) ? strings::Failed : strings::SucceededValue, nullptr, &output);
            } else simple(block, type + strings::Colon + kind);
            ++index;
        }
        return;
    }
    if (type == strings::Result && !value.value(strings::IsError, false)) {
        if (value.contains(strings::StructuredOutputValue)) finalResponse(value[strings::StructuredOutputValue], true);
        else if (value.contains(strings::Result) && value[strings::Result].is_string() && !str(value, strings::Result).isEmpty()) finalResponse(value[strings::Result]);
        else simple(value);
        return;
    }
    simple(value);
}
}
