#pragma once
#include "core/agents/definition.h"

namespace an {
AgentDefinition codexAgent();
std::unique_ptr<AgentCatalogProtocol> codexCatalog();
std::unique_ptr<AgentProgressProtocol> codexProgress();
}
