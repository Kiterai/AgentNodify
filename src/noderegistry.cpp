#include "constants.h"
#include "strings.h"
#include "nodedefinition.h"
#ifdef AGENTNODIFY_NODE_UI
#include "nodeform.h"
#include "nodeparameterform.h"
#include "modelcatalog.h"
#include "theme.h"
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
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QGridLayout>
#include <algorithm>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace an {
#include "nodecommon/formhelpers.inc"
#include "nodes/agent/form.inc"
#include "nodes/script/form.inc"
#include "nodes/shell/form.inc"
#include "nodecommon/hostform.inc"
#include "nodes/delay/form.inc"
#include "nodes/schedule/form.inc"
#include "nodes/user_input/form.inc"
#include "nodecommon/form.inc"
#include "nodes/branch/form.inc"
std::unique_ptr<NodeParameterForm> NodeRegistry::createForm(NodeForm &form, QVBoxLayout *layout, QFormLayout *fields, ModelCatalog *catalog) {
    switch (form.node().kind) {
    case Kind::Start: case Kind::End: return {};
    case Kind::Agent: return std::make_unique<AgentForm>(form, layout, fields, catalog);
    case Kind::All: case Kind::Any: return std::make_unique<InputPortsForm>(form, fields);
    case Kind::Branch: return std::make_unique<BranchForm>(form, layout);
    case Kind::Script: return std::make_unique<ScriptForm>(form, fields);
    case Kind::Shell: return std::make_unique<ShellForm>(form, layout, fields);
    case Kind::ProjectCreate: case Kind::FlowRun: case Kind::FlowWait: return std::make_unique<HostForm>(form, fields);
    case Kind::UserInput: return std::make_unique<UserInputForm>(form, layout);
    case Kind::Delay: return std::make_unique<DelayForm>(form, fields);
    case Kind::Schedule: return std::make_unique<ScheduleForm>(form, fields);
    }
    return {};
}
}
#else
#include "nodedefinition.h"
#include "runner.h"
#include "coordinator.h"
#include "schedule.h"
#include "cli.h"
#include <QTimeZone>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QUuid>
#include <algorithm>
#include <stdexcept>
namespace an {
#include "nodes/start/definition.inc"
#include "nodes/end/definition.inc"
#include "nodes/agent/definition.inc"
#include "nodes/agent/process.inc"
#include "nodes/all/definition.inc"
#include "nodes/branch/definition.inc"
#include "nodes/branch/runtime.inc"
#include "nodes/script/definition.inc"
#include "nodes/script/process.inc"
#include "nodes/shell/definition.inc"
#include "nodes/shell/process.inc"
#include "nodes/project_create/definition.inc"
#include "nodes/flow_run/definition.inc"
#include "nodes/flow_wait/definition.inc"
#include "nodes/any/definition.inc"
#include "nodes/user_input/definition.inc"
#include "nodes/user_input/runtime.inc"
#include "nodes/delay/definition.inc"
#include "nodes/delay/runtime.inc"
#include "nodes/schedule/definition.inc"
#include "nodes/schedule/runtime.inc"
#include "nodecommon/runtime.inc"
#include "nodes/project_create/host.inc"
#include "nodes/flow_run/host.inc"
#include "nodes/flow_wait/host.inc"

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
