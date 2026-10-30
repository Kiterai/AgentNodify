#pragma once
#include "core/agents/definition.h"

namespace an {
AgentDefinition claudeCodeAgent();
std::unique_ptr<AgentCatalogProtocol> claudeCodeCatalog();
std::unique_ptr<AgentProgressProtocol> claudeCodeProgress();
}
