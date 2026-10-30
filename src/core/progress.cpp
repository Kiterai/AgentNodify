#include "shared/constants.h"
#include "shared/strings.h"
#include "progress.h"
#include "agents/definition.h"
#include <QDir>
#include <algorithm>
#include <stdexcept>

namespace an {
namespace {
QString str(const Json &j, const char *key) {
    return agentText(j,key);
}
QString stringValue(const Json &j) { return agentStringValue(j); }
Json expandedInput(const Json &value) {
    return expandedAgentInput(value);
}
bool equivalent(const Json &a, const Json &b) {
    return a == b || expandedInput(a) == expandedInput(b) || (a.is_string() && b.is_string() && stringValue(a).trimmed() == stringValue(b).trimmed());
}
}
QString progressStatus(const QString &status) {
    if (status == strings::SucceededValue) return strings::Succeeded;
    if (status == strings::Failed) return strings::FailedLabel;
    if (status == strings::CancelledValue) return strings::Cancelled;
    if (status == strings::Unknown) return strings::UnknownResultLabel;
    return strings::Running;
}
ProgressParser::ProgressParser() {
    for(const auto &definition : agentDefinitions()) protocols_.push_back(definition.progress());
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
    for(const auto &protocol : protocols_) {
        key=protocol->fallbackKey(value); if(!key.isEmpty()) break;
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
    for(const auto &protocol : protocols_) {
        const auto label=protocol->commandLabel(key,entry.name);
        if(!label.isEmpty()) {entry.commandLabel=label;break;}
    }
    if (!entry.commandLabel.isEmpty()) entry.command = str(expandedInput(entry.input), entry.commandLabel == strings::Glob ? strings::Pattern : strings::Command);
    // Copy before adding file rows, which may reallocate items_.
    const auto toolName = entry.name; const auto arguments = entry.input; const auto effectiveStatus = entry.status;
    files(key, toolName, arguments, effectiveStatus);
    summarizeFileTool(key);
}
void ProgressParser::summarizeFileTool(const QString &key) {
    auto &entry = item(key, ProgressKind::Tool);
    if (std::none_of(protocols_.begin(),protocols_.end(),[&](const auto &protocol){return protocol->summarizeFile(key,entry.name);})) return;
    for (auto &file : items_) if (file.kind == ProgressKind::FileAccess && file.key.startsWith(key + strings::FileEventSeparator)) {
        entry.hidden = true;
        if (entry.spaceBefore) { file.spaceBefore = true; entry.spaceBefore = false; }
        return;
    }
}
void ProgressParser::event(const Json &value) {
    for(const auto &protocol : protocols_) if(protocol->receive(value,*this)) return;
    simple(value);
}
}
