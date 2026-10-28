void NodeForm::buildAgent(QVBoxLayout *layout, QFormLayout *form) {
    const auto &node = node_;
    agent_ = new QComboBox;
    agent_->setObjectName(strings::NodeAgent);
    agent_->addItems({strings::Codex, strings::ClaudeCode, strings::OpenCode});
    agent_->setCurrentText(node.agent);
    auto *combo = new ModelCombo;
    model_ = combo;
    model_->setObjectName(strings::NodeModel);
    model_->setEditable(false);
    model_->setPlaceholderText(strings::SelectModel);
    model_->setMinimumContentsLength(metrics::ComboMinimumCharacters);
    model_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    styleComboBoxPopup(agent_);
    styleComboBoxPopup(model_);
    auto *modelRow = new QWidget;
    auto *modelLayout = new QHBoxLayout(modelRow);
    modelLayout->setContentsMargins(0, 0, 0, 0);
    modelLayout->setSpacing(metrics::SpaceSmall);
    modelLayout->addWidget(model_, 1);
    refreshModels_ = new QPushButton(strings::RefreshSymbol);
    refreshModels_->setObjectName(strings::RefreshModels);
    refreshModels_->setFixedWidth(metrics::RefreshButtonWidth);
    modelLayout->addWidget(refreshModels_);
    modelStatus_ = new QLabel;
    modelStatus_->setObjectName(strings::ModelCatalogStatus);
    form->addRow(strings::AgentName, agent_);
    form->addRow(strings::ModelLabel, modelRow);
    form->addRow(strings::Empty, modelStatus_);
    effort_ = new QComboBox;
    effort_->setObjectName(strings::NodeEffort);
    styleComboBoxPopup(effort_);
    effort_->addItem(strings::DefaultEffortLabel, strings::Empty);
    populateEfforts();
    effort_->setToolTip(strings::EffortTooltip);
    form->addRow(strings::EffortLabel, effort_);
    retainContext_ = new NodeCheckBox(strings::RetainContextLabel);
    retainContext_->setObjectName(strings::RetainContext);
    retainContext_->setChecked(node.retainContext);
    retainContext_->setToolTip(strings::RetainContextTooltip);
    form->addRow(retainContext_);
    connect(retainContext_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!updating_) {
            node_.retainContext = enabled;
            publish();
        }
    });
    connect(effort_, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_) {
            node_.effort = effort_->currentData().toString();
            publish();
        }
    });
    populateModels();
    combo->beforePopup = [this] {
        if (catalog_)
            catalog_->request(node_.agent);
    };
    connect(refreshModels_, &QPushButton::clicked, this, [this] {
        if (catalog_)
            catalog_->request(node_.agent, true);
    });
    if (catalog_) {
        connect(catalog_, &ModelCatalog::changed, this, [this](const QString &agent) {
            if (agent == node_.agent)
                populateModels();
        });
        QTimer::singleShot(0, this, [this] {
            if (catalog_)
                catalog_->request(node_.agent);
        });
    }
    connect(agent_, &QComboBox::currentTextChanged, this, [this](const QString &agent) {
        if (updating_)
            return;
        node_.agent = agent;
        node_.model.clear();
        node_.effort.clear();
        effort_->setCurrentIndex(0);
        populateModels();
        publish();
        if (catalog_)
            catalog_->request(agent);
    });
    connect(model_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_)
            return;
        if (index < 0)
            return;
        node_.model = model_->currentData().toString();
        populateEfforts();
        publish();
    });
    prompt_ = addPreview(layout, strings::InputPromptLabel, strings::Prompt, node.prompt, strings::PromptPreviewPlaceholder);
    schema_ = addPreview(layout, strings::OutputSchemaLabel, strings::Schema, node.schema, strings::SchemaPreviewPlaceholder);
    syncParameters_ = [this](const Node &previous) {
        const auto &node = node_;
        const bool modelChanged = previous.agent != node.agent || previous.model != node.model;
        retainContext_->setChecked(node.retainContext);
        if (agent_) {
            agent_->setCurrentText(node.agent);
            if (modelChanged)
                populateModels();
        }
        if (prompt_ && prompt_->toPlainText() != node.prompt)
            prompt_->setPlainText(node.prompt);
        if (schema_ && schema_->toPlainText() != node.schema)
            schema_->setPlainText(node.schema);
        if (effort_) {
            if (effort_->findData(node.effort) < 0)
                effort_->addItem(node.effort, node.effort);
            effort_->setCurrentIndex(effort_->findData(node.effort));
        }
    };
    readOnlyParameters_ = [this](bool readOnly) { retainContext_->setEnabled(!readOnly); };
}

void NodeForm::populateModels() {
    if (!model_)
        return;
    QSignalBlocker block(model_);
    model_->clear();
    const auto entry = catalog_ ? catalog_->entry(node_.agent) : CatalogEntry{};
    for (auto &info : entry.models) {
        const auto label = info.name;
        model_->addItem(label, info.id);
        model_->setItemData(model_->count() - 1, info.id + strings::Newline + info.description, Qt::ToolTipRole);
    }
    auto selected = model_->findData(node_.model);
    if (selected < 0 && !node_.model.isEmpty()) {
        model_->addItem(node_.model + strings::CurrentSettingSuffix, node_.model);
        selected = model_->count() - 1;
    }
    model_->setCurrentIndex(selected);
    QString status = strings::CatalogNotLoaded;
    QString tooltip = strings::SelectCatalogModelHint;
    if (entry.state == CatalogState::Loading)
        status = strings::CatalogLoading;
    else if (entry.state == CatalogState::Ready)
        status.clear();
    else if (entry.state == CatalogState::Error) {
        status = entry.models.empty() ? strings::CatalogFailed : strings::CatalogPreviousModels;
        tooltip = entry.error + strings::Newline + tooltip;
    }
    modelStatus_->setText(status);
    modelStatus_->setToolTip(tooltip);
    model_->setToolTip(tooltip);
    parameterForm_->setRowVisible(modelStatus_, !status.isEmpty());
    refreshModels_->setEnabled(catalog_ && entry.state != CatalogState::Loading);
    refreshModels_->setToolTip(strings::RefreshModelsTooltip + tooltip);
    populateEfforts();
    scheduleResize();
}
void NodeForm::populateEfforts() {
    if (!effort_)
        return;
    QSignalBlocker blocker(effort_);
    effort_->clear();
    effort_->addItem(strings::DefaultEffortLabel, strings::Empty);
    QStringList levels = node_.agent == strings::ClaudeCode
                             ? QStringList{strings::Low, strings::Medium, strings::High, strings::Xhigh, strings::Max}
                             : QStringList{strings::Low, strings::Medium, strings::High, strings::Xhigh};
    if (catalog_)
        for (const auto &info : catalog_->entry(node_.agent).models)
            if (info.id == node_.model && !info.efforts.isEmpty()) {
                levels = info.efforts;
                break;
            }
    for (const auto &value : levels) {
        QString label = value;
        if (!label.isEmpty())
            label[0] = label[0].toUpper();
        effort_->addItem(label, value);
    }
    if (effort_->findData(node_.effort) < 0)
        effort_->addItem(node_.effort + strings::CurrentSettingSuffix, node_.effort);
    effort_->setCurrentIndex(effort_->findData(node_.effort));
}
