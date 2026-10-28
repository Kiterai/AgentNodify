#include "nodedefinition.h"
#include "shared/strings.h"
#include <algorithm>
namespace an {
Json schemaSources(const std::vector<Json> &values) {
    Json alternatives = Json::array();
    for (auto &value : values)
        if (!value.is_null() && std::find(alternatives.begin(), alternatives.end(), value) == alternatives.end())
            alternatives.push_back(value);
    return alternatives.empty() ? Json() : Json{{strings::AnSources, alternatives}};
}
std::vector<int> remapNodeOutputs(const Node &old, const Node &node, std::optional<unsigned> removed) {
    const auto &before = nodeDefinition(old.kind);
    const auto &after = nodeDefinition(node.kind);
    const auto oldOutputs = before.outputs(old), newOutputs = after.outputs(node);
    std::vector<int> ports(oldOutputs, -1);
    for (unsigned i = 0; i < std::min(oldOutputs, newOutputs); ++i)
        ports[i] = int(i);
    if (old.kind != node.kind || !after.editableOutputs)
        return ports;
    const auto oldCount = before.editableOutputCount(old), newCount = after.editableOutputCount(node);
    for (unsigned i = 0; i < oldCount; ++i)
        ports[i] = i < newCount ? int(i) : -1;
    if (newCount < oldCount) {
        if (removed && oldCount == newCount + 1 && *removed < oldCount) {
            for (unsigned i = 0; i < oldCount; ++i)
                ports[i] = i == *removed ? -1 : int(i > *removed ? i - 1 : i);
        } else {
            std::vector<int> matched(oldCount, -1);
            unsigned next = 0;
            for (unsigned i = 0; i < oldCount && next < newCount; ++i)
                if (after.sameOutput(old, i, node, next))
                    matched[i] = int(next++);
            if (next == newCount)
                for (unsigned i = 0; i < oldCount; ++i)
                    ports[i] = matched[i];
        }
    }
    if (after.trailingOutput)
        ports[oldCount] = int(newCount);
    return ports;
}
} // namespace an
