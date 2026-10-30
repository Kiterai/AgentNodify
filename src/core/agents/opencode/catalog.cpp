#include "agent.h"
#include <QRegularExpression>
#include <stdexcept>

namespace an {
namespace {
class OpenCodeCatalog final : public AgentCatalogProtocol {
public:
    QStringList arguments(const AgentConfig &config) const override {
        QStringList args{strings::Models}; if(config.opencodeStandalone) args << strings::StandaloneArgument; return args;
    }
    bool lineBased() const override {return false;}
    CatalogAction started() override {return {{},true,false};}
    void complete(const QByteArray &data) override {
        auto output=QString::fromUtf8(data); output.remove(QRegularExpression(strings::AnsiEscapePattern));
        const QRegularExpression pattern(strings::OpenCodeModelPattern);
        for(const auto &line : output.split('\n')) {
            const auto id=line.trimmed(); if(id.isEmpty()) continue;
            if(id.size()>limits::ModelIdCharacters || !pattern.match(id).hasMatch()) throw std::runtime_error(strings::InvalidOpenCodeCatalog);
            addModel({id,id,{}});
        }
    }
};
}
std::unique_ptr<AgentCatalogProtocol> openCodeCatalog() {return std::make_unique<OpenCodeCatalog>();}
}
