// ProjectCreate, FlowRun and FlowWait share the same target/path editor.
class HostForm final : public NodeParameterForm {
public:
    HostForm(NodeForm &owner, QFormLayout *form);
    void setFlowChoices(const std::vector<std::pair<QString, QString>> &choices) override;
private:
    QComboBox *targetFlow_ = nullptr;
    QLineEdit *projectPath_ = nullptr;
    std::vector<std::pair<QString, QString>> flowChoices_;
    void refreshTargetFlows();
};

HostForm::HostForm(NodeForm &owner, QFormLayout *form) : NodeParameterForm(owner) {
    const auto &node = node_;
    targetFlow_ = new QComboBox;
    targetFlow_->setObjectName(strings::TargetFlow);
    targetFlow_->setPlaceholderText(strings::SelectFlow);
    styleComboBoxPopup(targetFlow_);
    targetFlow_->setMinimumContentsLength(metrics::ComboMinimumCharacters);
    targetFlow_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    projectPath_ = new QLineEdit(node.projectPath);
    projectPath_->setObjectName(strings::ProjectPath);
    projectPath_->setMinimumWidth(0);
    projectPath_->setPlaceholderText(strings::ProjectPathPlaceholder);
    projectPath_->setToolTip(strings::ProjectPathTooltip);
    form->addRow(strings::FlowLabel, targetFlow_);
    form->addRow(strings::ProjectPathLabel, projectPath_);
    setFlowChoices({});
    syncParameters_ = [this](const Node &) {
        setFlowChoices(flowChoices_);
        if (projectPath_->text() != node_.projectPath)
            projectPath_->setText(node_.projectPath);
    };
    connect(targetFlow_, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_) {
            node_.targetFlow = targetFlow_->currentData().toString();
            publish();
        }
    });
    connect(projectPath_, &QLineEdit::textEdited, this, [this](const QString &text) {
        node_.projectPath = text;
        publish();
    });
}

void HostForm::setFlowChoices(const std::vector<std::pair<QString, QString>> &choices) {
    flowChoices_ = choices;
    refreshTargetFlows();
}

void HostForm::refreshTargetFlows() {
    const auto &choices = flowChoices_;
    if (!targetFlow_)
        return;
    QSignalBlocker block(targetFlow_);
    targetFlow_->clear();
    for (const auto &[path, name] : choices) {
        targetFlow_->addItem(name, path);
        targetFlow_->setItemData(targetFlow_->count() - 1, path, Qt::ToolTipRole);
    }
    auto index = targetFlow_->findData(node_.targetFlow);
    if (index < 0 && !node_.targetFlow.isEmpty()) {
        targetFlow_->addItem(node_.targetFlow + strings::UnregisteredSuffix, node_.targetFlow);
        index = targetFlow_->count() - 1;
    }
    targetFlow_->setCurrentIndex(index);
}
