#pragma once
#include "core/agents/definition.h"

namespace an {
AgentDefinition openCodeAgent();
std::unique_ptr<AgentCatalogProtocol> openCodeCatalog();
std::unique_ptr<AgentProgressProtocol> openCodeProgress();
}
