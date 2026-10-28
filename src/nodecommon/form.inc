void NodeForm::buildInputs(QVBoxLayout *, QFormLayout *form) {
    const auto &node = node_;
    inputs_ = new QSpinBox;
    inputs_->setObjectName(strings::NodeInputs);
    inputs_->setRange(1, limits::InputPorts);
    inputs_->setValue(int(node.inputs));
    inputs_->setToolTip(strings::InputCountTooltip);
    form->addRow(strings::InputCountLabel, inputs_);
    connect(inputs_, &QSpinBox::valueChanged, this, [this](int count) {
        if (!updating_) {
            node_.inputs = unsigned(count);
            publish();
        }
    });
    syncParameters_ = [this](const Node &) { inputs_->setValue(int(node_.inputs)); };
}
