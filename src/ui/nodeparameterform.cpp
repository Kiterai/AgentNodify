#include "nodeparameterform.h"
#include "nodeform.h"

namespace an {
NodeParameterForm::NodeParameterForm(NodeForm &owner)
    : QObject(&owner), owner_(owner), node_(owner.node_), readOnly_(owner.readOnly_), updating_(owner.updating_) {}

void NodeParameterForm::synchronize(const Node &previous) {
    if (syncParameters_) syncParameters_(previous);
}
void NodeParameterForm::applyReadOnly(bool readOnly) {
    if (readOnlyParameters_) readOnlyParameters_(readOnly);
}
void NodeParameterForm::setFlowChoices(const std::vector<std::pair<QString, QString>> &) {}
void NodeParameterForm::publish() { owner_.publish(); }
void NodeParameterForm::scheduleResize() { owner_.scheduleResize(); }
void NodeParameterForm::setReadOnly(bool readOnly) { owner_.setReadOnly(readOnly); }
void NodeParameterForm::outputRemoved(unsigned index) { emit owner_.outputRemoved(index); }
TextPreview *NodeParameterForm::addPreview(QVBoxLayout *layout, const QString &label, const QString &field, const QString &text, const QString &placeholder) {
    return owner_.addPreview(layout, label, field, text, placeholder);
}
}
