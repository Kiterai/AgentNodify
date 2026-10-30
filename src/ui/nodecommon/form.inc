// All and Any have exactly the same input-count editor and no other UI state.
class InputPortsForm final : public NodeParameterForm {
public:
    InputPortsForm(NodeForm &owner, QFormLayout *form);
private:
    QSpinBox *inputs_ = nullptr;
};

InputPortsForm::InputPortsForm(NodeForm &owner, QFormLayout *form) : NodeParameterForm(owner) {
    const auto &node = node_;
    inputs_ = new QSpinBox;
    inputs_->setObjectName(strings::NodeInputs);
    inputs_->setRange(1, limits::InputPorts);
    inputs_->setValue(int(node.inputs));
    inputs_->setToolTip(strings::InputCountTooltip);
    owner.setFixedWidth(metrics::JoinNodeWidth);
    form->addRow(inputs_);
    connect(inputs_, &QSpinBox::valueChanged, this, [this](int count) {
        if (!updating_) {
            node_.inputs = unsigned(count);
            publish();
        }
    });
    syncParameters_ = [this](const Node &) { inputs_->setValue(int(node_.inputs)); };
}
