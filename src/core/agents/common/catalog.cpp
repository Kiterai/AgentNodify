#include "catalog.h"
#include <algorithm>
#include <stdexcept>

namespace an {
QString agentText(const Json &value, const char *key) {
    return value.is_object() && value.contains(key) && value.at(key).is_string()
        ? QString::fromStdString(value.at(key).get<std::string>()) : QString();
}
void AgentCatalogProtocol::addModel(ModelInfo model) {
    if(std::any_of(models_.begin(),models_.end(),[&](const auto &existing){return existing.id==model.id;})) return;
    models_.push_back(std::move(model));
    if(models_.size()>limits::CatalogModels) throw std::runtime_error(QString(strings::TooManyModels).arg(limits::CatalogModels).toStdString());
}
void AgentCatalogProtocol::addModels(const Json &models, const char *idKey, const char *effortKey, const char *fallbackId) {
    if(!models.is_array()) throw std::runtime_error(strings::InvalidCatalogResponse);
    for(const auto &model : models) {
        if(!model.is_object()) throw std::runtime_error(strings::InvalidCatalogModel);
        if(model.value(strings::Hidden,false)) continue;
        auto id=agentText(model,idKey);
        if(id.isEmpty() && fallbackId) id=agentText(model,fallbackId);
        if(id.trimmed().isEmpty() || id.size()>limits::ModelIdCharacters || id.contains('\n') || id.contains('\r')) throw std::runtime_error(strings::InvalidModelId);
        if(std::any_of(models_.begin(),models_.end(),[&](const auto &existing){return existing.id==id;})) continue;
        auto name=agentText(model,strings::DisplayName); if(name.isEmpty()) name=id;
        auto description=agentText(model,strings::Description);
        const auto resolved=agentText(model,strings::ResolvedModel); if(!resolved.isEmpty()) description+=strings::Newline+resolved;
        ModelInfo info{id,name.left(limits::ModelNameCharacters),description.left(limits::ModelDescriptionCharacters)};
        if(model.contains(effortKey) && model.at(effortKey).is_array()) for(const auto &effort : model.at(effortKey)) {
            auto level=effort.is_string() ? QString::fromStdString(effort.get<std::string>()) : effort.is_object() ? agentText(effort,strings::ReasoningEffort) : QString();
            if(level.isEmpty() && effort.is_object()) level=agentText(effort,strings::Effort);
            if(!level.isEmpty() && level.size()<=limits::EffortCharacters && !info.efforts.contains(level)) info.efforts.append(level);
        }
        addModel(std::move(info));
    }
}
}
