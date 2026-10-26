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
    if (status == "completed" || status == "success" || status == "succeeded") return "succeeded";
    if (status == "failed" || status == "error") return "failed";
    if (status == "cancelled" || status == "canceled") return "cancelled";
    return "running";
}
QString accessOf(const QString &kind) {
    if (kind == "add" || kind == "added" || kind == "create") return "作成";
    if (kind == "delete" || kind == "deleted" || kind == "remove") return "削除";
    if (kind == "rename" || kind == "renamed" || kind == "move") return "移動";
    return "変更";
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
            if (depth > 128) throw std::runtime_error("display depth limit"); return true;
        }, false);
    } catch (...) { return Json(Json::value_t::discarded); }
}
QString progressStatus(const QString &status) {
    if (status == "succeeded") return "完了";
    if (status == "failed") return "失敗";
    if (status == "cancelled") return "中止";
    if (status == "unknown") return "結果不明";
    return "実行中";
}
ProgressItem &ProgressParser::item(const QString &key, ProgressKind kind) {
    auto it = indexes_.find(key);
    if (it != indexes_.end()) return items_[it.value()];
    indexes_[key] = items_.size(); items_.push_back({}); auto &result = items_.back(); result.key = key; result.kind = kind; return result;
}
void ProgressParser::append(const QString &chunk) {
    pending_ += chunk;
    qsizetype start = 0, end;
    while ((end = pending_.indexOf('\n', start)) >= 0) { line(pending_.mid(start, end - start)); start = end + 1; }
    pending_.remove(0, start);
}
void ProgressParser::finish(State state) {
    if (!pending_.isEmpty()) { const auto rest = std::move(pending_); pending_.clear(); line(rest); }
    for (auto &entry : items_) if ((entry.kind == ProgressKind::Tool || entry.kind == ProgressKind::FileAccess) && entry.status == "running")
        entry.status = state == State::Cancelled ? "cancelled" : state == State::Failed ? "failed" : "unknown";
}
void ProgressParser::finalResponse(const Json &output, bool structured) {
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) if (it->kind == ProgressKind::Message && equivalent(it->content, output)) {
        const bool quotedJson = it->content.is_string() && displayJson(stringValue(it->content)).is_string();
        it->jsonContent = it->jsonContent || structured || quotedJson;
        it->name = "最終応答"; it->content = output; return;
    }
    auto &entry = item("final-response", ProgressKind::Message); entry.name = "最終応答"; entry.content = output; entry.jsonContent = structured;
}
void ProgressParser::line(const QString &text) {
    ++line_; if (text.trimmed().isEmpty()) return;
    if (text.startsWith("作業ディレクトリ: ")) directory_ = text.mid(QString("作業ディレクトリ: ").size()).trimmed();
    const auto value = displayJson(text);
    if (value.is_discarded() || !value.is_object()) {
        auto &entry = item(QString("plain:%1").arg(line_), ProgressKind::Simple); entry.text = text; return;
    }
    try { event(value); } catch (const std::exception &) { simple(value, "解釈できないイベント"); }
}
void ProgressParser::simple(const Json &value, const QString &label) {
    QString key;
    if (value.is_object() && value.contains("item") && !str(value["item"], "id").isEmpty()) key = "simple:item:" + str(value["item"], "id");
    else if (value.is_object() && value.contains("part")) {
        auto id = str(value["part"], "id"); if (id.isEmpty()) id = str(value["part"], "partID");
        if (!id.isEmpty()) key = "simple:part:" + id;
    }
    if (str(value, "type") == "stream_event" && value.contains("event") && value["event"].is_object()) {
        const auto &event = value["event"];
        const int index = event.contains("index") && event["index"].is_number_integer() ? event["index"].get<int>() : 0;
        key = "simple:stream:" + claudeMessage_ + ":" + QString::number(index) + ":" + str(event, "type");
    }
    if (key.isEmpty()) key = QString("event:%1:%2").arg(line_).arg(items_.size());
    auto &entry = item(key, ProgressKind::Simple);
    entry.name = label.isEmpty() ? str(value, "type") : label;
    if (entry.name.isEmpty()) entry.name = "イベント";
    entry.text = str(value, "message"); if (entry.text.isEmpty()) entry.text = str(value, "subtype");
    entry.content = value;
}
void ProgressParser::file(const QString &key, const QString &path, const QString &access, const QString &status) {
    if (path.isEmpty() || path.contains("://")) return; // A remote resource is not a filesystem access.
    auto normalized = path; normalized.replace('\\', '/');
    if (!directory_.isEmpty() && QDir::isRelativePath(normalized)) normalized = QDir(directory_).filePath(normalized);
    normalized = QDir::cleanPath(normalized);
    auto &entry = item(key + ":file:" + access + ":" + normalized, ProgressKind::FileAccess);
    entry.path = normalized; entry.name = access; entry.status = status;
}
void ProgressParser::files(const QString &key, const QString &name, const Json &raw, const QString &status) {
    const auto input = expandedInput(raw); if (!input.is_object()) return;
    const auto toolName = name.toLower().section('/', -1).section('.', -1).section("__", -1);
    QString access;
    if (toolName == "read" || toolName == "read_file") access = "読取";
    else if (toolName == "write" || toolName == "write_file") access = "書込";
    else if (toolName == "edit" || toolName == "multiedit" || toolName == "notebookedit") access = "変更";
    // A generic MCP read/write may target a database or remote resource. Require
    // a file-specific tool/argument before attributing it to the filesystem.
    if ((name.contains('/') || name.contains("__")) && !toolName.endsWith("_file") &&
        !input.contains("file_path") && !input.contains("filePath") && !input.contains("notebook_path")) access.clear();
    if (!access.isEmpty()) {
        auto path = str(input, "file_path"); if (path.isEmpty()) path = str(input, "filePath");
        if (path.isEmpty()) path = str(input, "path"); if (path.isEmpty()) path = str(input, "notebook_path");
        file(key, path, access, status);
    }
    if (toolName == "patch" || toolName == "apply_patch") {
        auto patch = str(input, "patchText"); if (patch.isEmpty()) patch = str(input, "patch");
        for (const auto &line : patch.split('\n')) {
            if (line.startsWith("*** Add File: ")) file(key, line.mid(14).trimmed(), "作成", status);
            else if (line.startsWith("*** Delete File: ")) file(key, line.mid(17).trimmed(), "削除", status);
            else if (line.startsWith("*** Update File: ")) file(key, line.mid(17).trimmed(), "変更", status);
            else if (line.startsWith("*** Move to: ") || line.startsWith("*** Move To: ")) file(key, line.mid(13).trimmed(), "移動", status);
        }
    }
}
void ProgressParser::tool(const QString &key, const QString &name, const QString &status, const Json *input, const Json *output) {
    auto &entry = item(key, ProgressKind::Tool);
    if (!name.isEmpty()) entry.name = name;
    if (entry.name.isEmpty()) entry.name = "ツール";
    if (entry.status.isEmpty() || entry.status == "running" || status != "running") entry.status = status;
    if (input) { entry.input = *input; entry.hasInput = true; }
    if (output) { entry.output = *output; entry.hasOutput = true; }
    // Copy before adding file rows, which may reallocate items_.
    const auto toolName = entry.name; const auto arguments = entry.input; const auto effectiveStatus = entry.status;
    files(key, toolName, arguments, effectiveStatus);
}
void ProgressParser::event(const Json &value) {
    const auto type = str(value, "type");
    if (type == "assistant" || type == "user" || type == "result" || type == "stream_event") { claude(value); return; }
    if (type.startsWith("item.") && value.contains("item") && value["item"].is_object()) {
        const auto &data = value["item"]; const auto kind = str(data, "type");
        auto id = str(data, "id"); if (id.isEmpty()) id = QString::number(line_);
        const auto key = "codex:" + id;
        auto status = statusOf(str(data, "status"));
        if (str(data, "status").isEmpty() && type == "item.completed") status = "succeeded";
        if (kind == "agent_message") {
            if (!data.contains("text") || !data["text"].is_string()) { simple(value); return; }
            auto &entry = item(key, ProgressKind::Message); entry.name = "エージェント"; entry.content = str(data, "text").toStdString(); return;
        }
        if (kind == "command_execution") {
            if (data.contains("exit_code") && data["exit_code"].is_number_integer() && data["exit_code"] != 0) status = "failed";
            const Json input = {{"command", data.value("command", Json())}};
            const Json *output = data.contains("aggregated_output") ? &data["aggregated_output"] : nullptr;
            tool(key, "command_execution", status, &input, output); return;
        }
        if (kind == "mcp_tool_call") {
            const auto name = str(data, "server") + "/" + str(data, "tool");
            const Json *input = data.contains("arguments") ? &data["arguments"] : nullptr;
            const Json *output = data.contains("error") && !data["error"].is_null() ? &data["error"] : data.contains("result") ? &data["result"] : nullptr;
            if (data.contains("error") && !data["error"].is_null()) status = "failed";
            tool(key, name, status, input, output); return;
        }
        if (kind == "web_search") {
            const Json input = {{"query", data.value("query", Json())}};
            const Json *output = data.contains("result") ? &data["result"] : nullptr;
            tool(key, "web_search", status, &input, output); return;
        }
        if (kind == "file_change" && data.contains("changes") && data["changes"].is_array()) {
            bool found = false;
            for (const auto &change : data["changes"]) if (!str(change, "path").isEmpty()) { file(key, str(change, "path"), accessOf(str(change, "kind")), status); found = true; }
            if (!found) simple(value, kind);
            return;
        }
        simple(value, kind); return;
    }
    if ((type == "text" || type == "tool_use") && value.contains("part") && value["part"].is_object()) {
        const auto &part = value["part"]; auto id = str(part, "id");
        if (id.isEmpty()) id = str(part, "partID"); if (id.isEmpty()) id = QString::number(line_);
        const auto key = "opencode:" + id;
        if (type == "text") {
            if (!part.contains("text") || !part["text"].is_string()) { simple(value); return; }
            auto &entry = item(key, ProgressKind::Message); entry.name = "エージェント"; entry.content = str(part, "text").toStdString(); return;
        }
        if (!part.contains("state") || !part["state"].is_object()) { simple(value); return; }
        const auto &state = part["state"]; const auto status = statusOf(str(state, "status"));
        const Json *input = state.contains("input") ? &state["input"] : nullptr;
        const Json *output = state.contains("error") ? &state["error"] : state.contains("output") ? &state["output"] : state.contains("content") ? &state["content"] : nullptr;
        if (!output && state.contains("metadata") && state["metadata"].is_object() && state["metadata"].contains("content")) output = &state["metadata"]["content"];
        tool(key, str(part, "tool"), status, input, output);
        // V2 CLI diagnostics can nest tool metadata one level deeper.
        if (state.contains("metadata") && state["metadata"].is_object()) {
            const auto &outer = state["metadata"];
            const auto &metadata = outer.contains("metadata") && outer["metadata"].is_object() ? outer["metadata"] : outer;
            if (metadata.contains("files") && metadata["files"].is_array()) for (const auto &change : metadata["files"]) {
                auto path = str(change, "file"); if (path.isEmpty()) path = str(change, "path");
                file(key, path, accessOf(str(change, "status")), status);
            }
        }
        return;
    }
    simple(value);
}
void ProgressParser::claude(const Json &value) {
    const auto type = str(value, "type");
    if (type == "stream_event" && value.contains("event") && value["event"].is_object()) {
        const auto &event = value["event"]; const auto kind = str(event, "type");
        if (kind == "message_start" && event.contains("message")) { claudeMessage_ = str(event["message"], "id"); blocks_.clear(); claudeBlock_ = -1; return; }
        const int index = event.value("index", 0);
        if (kind == "content_block_start" && event.contains("content_block")) {
            claudeBlock_ = index;
            const auto &block = event["content_block"]; const auto blockType = str(block, "type");
            if (blockType == "tool_use") {
                const auto key = "claude:tool:" + str(block, "id"); blocks_[index] = {key, {}, true};
                const Json input = block.value("input", Json::object()); tool(key, str(block, "name"), "running", &input, nullptr);
            } else if (blockType == "text") {
                const auto key = "claude:text:" + claudeMessage_ + ":" + QString::number(index); blocks_[index] = {key, {}, false};
                auto &entry = item(key, ProgressKind::Message); entry.name = "エージェント"; entry.content = str(block, "text").toStdString();
            } else simple(value, blockType);
            return;
        }
        if (kind == "content_block_delta" && event.contains("delta") && blocks_.contains(index)) {
            claudeBlock_ = index;
            auto &block = blocks_[index]; const auto &delta = event["delta"];
            if (!block.tool && str(delta, "type") == "text_delta") {
                auto &entry = item(block.key, ProgressKind::Message); entry.content = (stringValue(entry.content) + str(delta, "text")).toStdString();
            } else if (block.tool && str(delta, "type") == "input_json_delta") {
                block.input += str(delta, "partial_json"); const auto input = expandedInput(Json(block.input.toStdString()));
                const auto name = item(block.key, ProgressKind::Tool).name; tool(block.key, name, "running", &input, nullptr);
            } else simple(value);
            return;
        }
        if (kind == "content_block_stop") { claudeBlock_ = -1; return; }
        if (kind == "message_stop" || kind == "message_delta") return;
        simple(value); return;
    }
    if ((type == "assistant" || type == "user") && value.contains("message") && value["message"].is_object()) {
        const auto &message = value["message"];
        if (!message.contains("content") || !message["content"].is_array()) { simple(value); return; }
        const auto messageId = str(message, "id").isEmpty() ? QString::number(line_) : str(message, "id");
        int index = 0;
        for (const auto &block : message["content"]) {
            const auto kind = str(block, "type");
            if (type == "assistant" && kind == "text") {
                const auto text = str(block, "text"); QString key;
                const auto prefix = "claude:text:" + messageId + ":";
                if (messageId == claudeMessage_) {
                    const auto streamedIndex = message["content"].size() == 1 && claudeBlock_ >= 0 ? claudeBlock_ : index;
                    if (blocks_.contains(streamedIndex) && !blocks_[streamedIndex].tool) key = blocks_[streamedIndex].key;
                }
                // Without partial events, UUID identifies a complete message. Different
                // complete blocks may intentionally contain exactly the same text.
                if (key.isEmpty()) key = prefix + (str(value, "uuid").isEmpty() ? QString::number(line_) : str(value, "uuid")) + ":" + QString::number(index);
                auto &entry = item(key, ProgressKind::Message); entry.name = "エージェント"; entry.content = text.toStdString();
            } else if (kind == "tool_use") {
                const Json input = block.value("input", Json::object());
                tool("claude:tool:" + str(block, "id"), str(block, "name"), "running", &input, nullptr);
            } else if (kind == "tool_result") {
                const auto key = "claude:tool:" + str(block, "tool_use_id"); const Json output = block.value("content", Json());
                tool(key, {}, block.value("is_error", false) ? "failed" : "succeeded", nullptr, &output);
            } else simple(block, type + ":" + kind);
            ++index;
        }
        return;
    }
    if (type == "result" && !value.value("is_error", false)) {
        if (value.contains("structured_output")) finalResponse(value["structured_output"], true);
        else if (value.contains("result") && value["result"].is_string() && !str(value, "result").isEmpty()) finalResponse(value["result"]);
        else simple(value);
        return;
    }
    simple(value);
}
}
