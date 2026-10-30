#include "agent.h"
#include <QSet>
#include <stdexcept>

namespace an {
namespace {
class CodexCatalog final : public AgentCatalogProtocol {
    int requestId_=1;
    bool initialized_=false;
    QSet<QString> seenCursors_;
public:
    QStringList arguments(const AgentConfig &) const override {return {strings::AppServer};}
    CatalogAction started() override {
        return {{{{strings::Id,1},{strings::Method,strings::Initialize},{strings::Params,{{strings::ClientInfo,{{strings::Name,strings::ApplicationName},{strings::Version,strings::ApplicationVersion}}}}}}}};
    }
    CatalogAction receive(const Json &value) override {
        if(value.contains(strings::Method) && value.contains(strings::Id))
            return {{{{strings::Id,value[strings::Id]},{strings::ErrorValue,{{strings::Code,defaults::RpcMethodNotFound},{strings::Message,strings::CatalogOnlyClient}}}}}};
        if(!value.contains(strings::Id) || value[strings::Id]!=requestId_) return {};
        if(value.contains(strings::ErrorValue)) throw std::runtime_error(strings::CodexCatalogRejected);
        const auto &result=value.at(strings::Result);
        if(!initialized_) {
            initialized_=true;
            return {{{{strings::Method,strings::Initialized}},{{strings::Id,++requestId_},{strings::Method,strings::ModelList},{strings::Params,{{strings::Limit,limits::CatalogPageSize}}}}}};
        }
        addModels(result.at(strings::Data),strings::Model,strings::SupportedReasoningEfforts,strings::Id);
        const auto cursor=agentText(result,strings::NextCursor);
        if(!cursor.isEmpty()) {
            if(seenCursors_.contains(cursor) || seenCursors_.size()>=limits::CatalogPages) throw std::runtime_error(strings::CatalogPaginationLimit);
            seenCursors_.insert(cursor);
            return {{{{strings::Id,++requestId_},{strings::Method,strings::ModelList},{strings::Params,{{strings::Limit,limits::CatalogPageSize},{strings::Cursor,cursor.toStdString()}}}}}};
        }
        return {{},false,true};
    }
};
}
std::unique_ptr<AgentCatalogProtocol> codexCatalog() {return std::make_unique<CodexCatalog>();}
}
