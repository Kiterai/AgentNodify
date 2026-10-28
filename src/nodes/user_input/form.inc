class UserInputForm final : public NodeParameterForm {
public:
    UserInputForm(NodeForm &owner, QVBoxLayout *layout);
private:
    QWidget *choices_ = nullptr;
    QVBoxLayout *choicesLayout_ = nullptr;
    QPushButton *addChoice_ = nullptr;
    void rebuildChoices();
};

UserInputForm::UserInputForm(NodeForm &owner, QVBoxLayout *layout) : NodeParameterForm(owner) {
    layout->addWidget(new QLabel(strings::UserChoicesLabel));
    choices_ = new QWidget;
    choices_->setObjectName(strings::UserChoices);
    choicesLayout_ = new QVBoxLayout(choices_);
    choicesLayout_->setContentsMargins(0, 0, 0, 0);
    choicesLayout_->setSpacing(metrics::SpaceSmall);
    layout->addWidget(choices_);
    addChoice_ = new QPushButton(strings::AddChoice);
    addChoice_->setObjectName(strings::AddChoiceValue);
    layout->addWidget(addChoice_);
    connect(addChoice_, &QPushButton::clicked, this, [this] {
        if (readOnly_)
            return;
        node_.userChoices.push_back(QString(strings::IndexedChoiceLabel).arg(node_.userChoices.size() + 1));
        rebuildChoices();
        publish();
    });
    rebuildChoices();
    syncParameters_ = [this](const Node &previous) {
        const auto &node = node_;
        if (previous.userChoices != node.userChoices)
            rebuildChoices();
    };
    readOnlyParameters_ = [this](bool readOnly) {
        addChoice_->setEnabled(!readOnly);
        for (auto *remove : choices_->findChildren<QPushButton *>(strings::RemoveChoice))
            remove->setEnabled(!readOnly && node_.userChoices.size() > 1);
    };
}

void UserInputForm::rebuildChoices() {
    while (auto *item = choicesLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    for (qsizetype i = 0; i < node_.userChoices.size(); ++i) {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *name = new QLineEdit(node_.userChoices[i]);
        name->setObjectName(strings::ChoiceName);
        name->setMinimumWidth(0);
        name->setToolTip(QString(strings::ChoiceNameTooltip).arg(i));
        layout->addWidget(name, 1);
        auto *remove = new QPushButton(strings::RemoveSymbol);
        remove->setObjectName(strings::RemoveChoice);
        remove->setFixedWidth(metrics::RemoveButtonWidth);
        layout->addWidget(remove);
        choicesLayout_->addWidget(row);
        connect(name, &QLineEdit::textEdited, this, [this, i](const QString &text) {
            if (!updating_ && !readOnly_ && i < node_.userChoices.size()) {
                node_.userChoices[i] = text;
                publish();
            }
        });
        connect(remove, &QPushButton::clicked, this, [this, i] {
            if (readOnly_ || node_.userChoices.size() <= 1 || i >= node_.userChoices.size())
                return;
            node_.userChoices.removeAt(i);
            rebuildChoices();
            emit outputRemoved(unsigned(i));
            scheduleResize();
        });
    }
    setReadOnly(readOnly_);
    scheduleResize();
}
