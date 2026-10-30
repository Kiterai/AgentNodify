#include "progress.h"
#include <stdexcept>

namespace an {
Json displayJson(const QString &text) {
    try {
        return Json::parse(text.toUtf8().toStdString(), [](int depth, Json::parse_event_t, Json &) {
            if(depth>limits::JsonComparisonDepth) throw std::runtime_error(strings::DisplayDepthLimit);
            return true;
        },false);
    } catch (...) {return Json(Json::value_t::discarded);}
}
QString agentStringValue(const Json &value) { return value.is_string() ? QString::fromStdString(value.get<std::string>()) : QString(); }
QString agentProgressStatus(const QString &status) {
    if(status==strings::Completed || status==strings::Success || status==strings::SucceededValue) return strings::SucceededValue;
    if(status==strings::Failed || status==strings::ErrorValue) return strings::Failed;
    if(status==strings::CancelledValue || status==strings::Canceled) return strings::CancelledValue;
    return strings::RunningValue;
}
QString agentFileAccess(const QString &kind) {
    if(kind==strings::Add || kind==strings::Added || kind==strings::Create) return strings::FileCreatedLabel;
    if(kind==strings::Delete || kind==strings::Deleted || kind==strings::Remove) return strings::FileDeletedLabel;
    if(kind==strings::Rename || kind==strings::Renamed || kind==strings::Move) return strings::FileMovedLabel;
    return strings::FileChangedLabel;
}
Json expandedAgentInput(const Json &value) {
    if(!value.is_string()) return value;
    auto parsed=displayJson(agentStringValue(value)); return parsed.is_discarded() ? value : parsed;
}
}
