#pragma once
#include "core/flow.h"
#include <functional>
#include <memory>

class QVBoxLayout;
class QFormLayout;

namespace an {
class ModelCatalog;
class NodeForm;
class NodeParameterForm;

// Presentation depends on the domain's Kind/Node, never the reverse.
struct NodePresentation {
    explicit NodePresentation(Kind kind);
    Kind kind;
    QString description;
    bool compact = false, specialBackground = false;
    QString background, foreground, accent;
    QStringList textFields;
    bool processOutput = false;
    std::function<QString(const Node &, bool, unsigned)> portCaption;
    std::function<std::unique_ptr<NodeParameterForm>(NodeForm &, QVBoxLayout *, QFormLayout *, ModelCatalog *)> createForm;
};

const std::vector<NodePresentation> &nodePresentations();
const NodePresentation &nodePresentation(Kind kind);
std::unique_ptr<NodeParameterForm> createNodeForm(NodeForm &, QVBoxLayout *, QFormLayout *, ModelCatalog *);
} // namespace an
