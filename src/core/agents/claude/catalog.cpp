#include "agent.h"
#include <stdexcept>

namespace an {
namespace {
class ClaudeCatalog final : public AgentCatalogProtocol {
public:
    QStringList arguments(const AgentConfig &) const override {
        return {strings::PrintArgument,strings::InputFormatArgument,strings::StreamJson,strings::OutputFormatArgument,strings::StreamJson,strings::VerboseArgument,strings::NoSessionPersistenceArgument,
            strings::SettingsArgument,strings::DisableHooksSettings,strings::StrictMcpArgument,strings::McpConfigArgument,strings::EmptyMcpSettings};
    }
    CatalogAction started() override {
        return {{{{strings::Type,strings::ControlRequest},{strings::RequestId,strings::CatalogInitialize},{strings::Request,{{strings::Subtype,strings::Initialize}}}}}};
    }
    CatalogAction receive(const Json &value) override {
        if(value.value(strings::Type,strings::Empty)==strings::ControlRequest)
            return {{{{strings::Type,strings::ControlResponse},{strings::Response,{{strings::Subtype,strings::ErrorValue},{strings::RequestId,value.at(strings::RequestId)},{strings::ErrorValue,strings::CatalogOnlyClient}}}}}};
        if(value.value(strings::Type,strings::Empty)!=strings::ControlResponse) return {};
        const auto &response=value.at(strings::Response);
        if(response.value(strings::RequestId,strings::Empty)!=strings::CatalogInitialize) return {};
        if(response.value(strings::Subtype,strings::Empty)!=strings::Success) throw std::runtime_error(strings::ClaudeInitializationFailed);
        addModels(response.at(strings::Response).at(strings::Models),strings::Value,strings::SupportedEffortLevels);
        return {{},false,true};
    }
};
}
std::unique_ptr<AgentCatalogProtocol> claudeCodeCatalog() {return std::make_unique<ClaudeCatalog>();}
}
