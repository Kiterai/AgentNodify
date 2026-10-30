#include "shared/constants.h"
#include "core/agents/definition.h"
#include "shared/strings.h"
#include <stdexcept>
#ifdef AGENTNODIFY_NODE_UI
#include "ui/nodepresentation.h"
#include "ui/nodeform.h"
#include "ui/nodeparameterform.h"
#include "core/modelcatalog.h"
#include "ui/theme.h"
#include <QComboBox>
#include <QCheckBox>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyle>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QGridLayout>
#include <algorithm>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

namespace an {
#include "ui/nodecommon/formhelpers.inc"
#include "ui/nodecommon/form.inc"
#include "ui/nodecommon/hostform.inc"
#include "nodes/agent/ui/form.inc"
#include "nodes/script/ui/form.inc"
#include "nodes/shell/ui/form.inc"
#include "nodes/delay/ui/form.inc"
#include "nodes/schedule/ui/form.inc"
#include "nodes/user_input/ui/form.inc"
#include "nodes/branch/ui/form.inc"
#include "nodes/start/ui/presentation.inc"
#include "nodes/end/ui/presentation.inc"
#include "nodes/agent/ui/presentation.inc"
#include "nodes/all/ui/presentation.inc"
#include "nodes/any/ui/presentation.inc"
#include "nodes/branch/ui/presentation.inc"
#include "nodes/script/ui/presentation.inc"
#include "nodes/shell/ui/presentation.inc"
#include "nodes/project_create/ui/presentation.inc"
#include "nodes/flow_run/ui/presentation.inc"
#include "nodes/flow_wait/ui/presentation.inc"
#include "nodes/user_input/ui/presentation.inc"
#include "nodes/delay/ui/presentation.inc"
#include "nodes/schedule/ui/presentation.inc"

const std::vector<NodePresentation> &nodePresentations() {
    static const std::vector<NodePresentation> presentations = {
        agentPresentation(), scriptPresentation(), shellPresentation(), allPresentation(), anyPresentation(),
        branchPresentation(), userInputPresentation(), delayPresentation(), schedulePresentation(), endPresentation(),
        projectCreatePresentation(), flowRunPresentation(), flowWaitPresentation(), startPresentation()
    };
    return presentations;
}
const NodePresentation &nodePresentation(Kind kind) {
    for (const auto &presentation : nodePresentations()) if (presentation.kind == kind) return presentation;
    throw std::invalid_argument(strings::UnknownNodeKind);
}
std::unique_ptr<NodeParameterForm> createNodeForm(NodeForm &form, QVBoxLayout *layout, QFormLayout *fields, ModelCatalog *catalog) {
    const auto &presentation = nodePresentation(form.node().kind);
    return presentation.createForm ? presentation.createForm(form, layout, fields, catalog) : nullptr;
}
}
#else
#include "core/nodedefinition.h"
#include "core/runner.h"
#include "core/coordinator.h"
#include "core/schedule.h"
#include "core/cli.h"
#include <QTimeZone>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QUuid>
#include <algorithm>
namespace an {
#include "nodes/start/core/definition.inc"
#include "nodes/end/core/definition.inc"
#include "nodes/agent/core/definition.inc"
#include "nodes/agent/core/process.inc"
#include "nodes/all/core/definition.inc"
#include "nodes/branch/core/definition.inc"
#include "nodes/branch/core/runtime.inc"
#include "nodes/script/core/definition.inc"
#include "nodes/script/core/process.inc"
#include "nodes/shell/core/definition.inc"
#include "nodes/shell/core/process.inc"
#include "nodes/project_create/core/definition.inc"
#include "nodes/flow_run/core/definition.inc"
#include "nodes/flow_wait/core/definition.inc"
#include "nodes/any/core/definition.inc"
#include "nodes/user_input/core/definition.inc"
#include "nodes/user_input/core/runtime.inc"
#include "nodes/delay/core/definition.inc"
#include "nodes/delay/core/runtime.inc"
#include "nodes/schedule/core/definition.inc"
#include "nodes/schedule/core/runtime.inc"
#include "core/nodecommon/runtime.inc"
#include "nodes/project_create/core/host.inc"
#include "nodes/flow_run/core/host.inc"
#include "nodes/flow_wait/core/host.inc"

const std::vector<NodeDefinition> &nodeDefinitions() {
    static const std::vector<NodeDefinition> definitions = {
        NodeRegistry::agent(),
        NodeRegistry::script(),
        NodeRegistry::shell(),
        NodeRegistry::all(),
        NodeRegistry::any(),
        NodeRegistry::branch(),
        NodeRegistry::userInput(),
        NodeRegistry::delay(),
        NodeRegistry::schedule(),
        NodeRegistry::end(),
        NodeRegistry::projectCreate(),
        NodeRegistry::flowRun(),
        NodeRegistry::flowWait(),
        NodeRegistry::start()
    };
    return definitions;
}
const NodeDefinition &nodeDefinition(Kind kind) {
    for (const auto &definition : nodeDefinitions()) if (definition.kind == kind) return definition;
    throw std::invalid_argument(strings::UnknownNodeKind);
}

}
#endif
