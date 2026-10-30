#pragma once
#include "core/flow.h"
#include "core/agents/config.h"
#include <memory>

namespace an {
struct ModelInfo {
    QString id, name, description;
    QStringList efforts;
    bool operator==(const ModelInfo &) const = default;
};
struct CatalogAction {
    std::vector<Json> messages;
    bool closeInput = false, finished = false;
};
// Protocol instances own discovery state; the transport owns processes/timers.
class AgentCatalogProtocol {
public:
    virtual ~AgentCatalogProtocol() = default;
    virtual QStringList arguments(const AgentConfig &) const = 0;
    virtual bool lineBased() const { return true; }
    virtual CatalogAction started() { return {}; }
    virtual CatalogAction receive(const Json &) { return {}; }
    virtual void complete(const QByteArray &) {}
    const std::vector<ModelInfo> &models() const { return models_; }
protected:
    std::vector<ModelInfo> models_;
    void addModels(const Json &models, const char *idKey, const char *effortKey, const char *fallbackId = nullptr);
    void addModel(ModelInfo model);
};
QString agentText(const Json &value, const char *key);
}
