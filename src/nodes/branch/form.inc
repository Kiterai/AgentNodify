void NodeForm::buildBranch(QVBoxLayout *layout, QFormLayout *) {
    rules_ = new QWidget;
    rules_->setObjectName(strings::NodeRules);
    rulesLayout_ = new QVBoxLayout(rules_);
    rulesLayout_->setContentsMargins(0, 0, 0, 0);
    rulesLayout_->setSpacing(metrics::SpaceMedium);
    layout->addWidget(rules_);
    rebuildRules();
    auto *add = new QPushButton(strings::AddRule);
    add->setObjectName(strings::AddRuleValue);
    layout->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this] {
        node_.rules.push_back({strings::Empty, strings::Empty, true});
        rebuildRules();
        publish();
    });
    syncParameters_ = [this](const Node &previous) {
        const auto &node = node_;
        bool rulesChanged = previous.rules.size() != node.rules.size();
        if (!rulesChanged)
            for (size_t i = 0; i < node.rules.size(); ++i) {
                const auto &a = previous.rules[i];
                const auto &b = node.rules[i];
                if (a.variable != b.variable || a.value != b.value || a.equals != b.equals)
                    rulesChanged = true;
            }
        if (rulesChanged)
            rebuildRules();
    };
}

void NodeForm::rebuildRules() {
    while (auto *item = rulesLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    for (size_t i = 0; i < node_.rules.size(); ++i) {
        auto *card = new QWidget;
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(metrics::SpaceSmall);
        auto *heading = new QHBoxLayout;
        heading->addWidget(new QLabel(QString(strings::IndexedRuleLabel).arg(i + 1)), 1);
        auto *remove = new QPushButton(strings::RemoveSymbol);
        remove->setObjectName(strings::RemoveRule);
        remove->setFixedWidth(metrics::RemoveButtonWidth);
        remove->setToolTip(strings::RemoveRuleTooltip);
        heading->addWidget(remove);
        layout->addLayout(heading);
        auto *variable = new QLineEdit(node_.rules[i].variable);
        variable->setMinimumWidth(0);
        variable->setObjectName(strings::RuleVariable);
        variable->setPlaceholderText(strings::RuleVariablePlaceholder);
        auto *subject = new QHBoxLayout;
        subject->addWidget(new QLabel(strings::IfLabel));
        subject->addWidget(variable, 1);
        subject->addWidget(new QLabel(strings::SubjectParticle));
        layout->addLayout(subject);
        auto *comparison = new QHBoxLayout;
        auto *op = new QComboBox;
        op->setObjectName(strings::RuleOperator);
        op->addItems({strings::EqualsLabel, strings::NotEqualsLabel});
        op->setCurrentIndex(node_.rules[i].equals ? 0 : 1);
        styleComboBoxPopup(op);
        auto *value = new QLineEdit(node_.rules[i].value);
        value->setMinimumWidth(0);
        value->setObjectName(strings::RuleValue);
        value->setPlaceholderText(strings::RuleValuePlaceholder);
        variable->installEventFilter(this);
        value->installEventFilter(this);
        op->installEventFilter(this);
        comparison->addWidget(value, 1);
        comparison->addWidget(new QLabel(strings::ComparisonParticle));
        comparison->addWidget(op);
        comparison->addWidget(new QLabel(strings::ThenLabel));
        layout->addLayout(comparison);
        rulesLayout_->addWidget(card);
        connect(variable, &QLineEdit::textEdited, card, [this, i](const QString &text) {
            if (i < node_.rules.size()) {
                node_.rules[i].variable = text;
                publish();
            }
        });
        connect(value, &QLineEdit::textEdited, card, [this, i](const QString &text) {
            if (i < node_.rules.size()) {
                node_.rules[i].value = text;
                publish();
            }
        });
        connect(op, &QComboBox::currentIndexChanged, card, [this, i](int index) {
            if (i < node_.rules.size()) {
                node_.rules[i].equals = index == 0;
                publish();
            }
        });
        connect(remove, &QPushButton::clicked, card, [this, i] {
            if (readOnly_ || i >= node_.rules.size())
                return;
            node_.rules.erase(node_.rules.begin() + i);
            rebuildRules();
            emit outputRemoved(unsigned(i));
            scheduleResize();
        });
    }
    rulesLayout_->addWidget(new QLabel(strings::ElseLabel));
    setReadOnly(readOnly_);
    scheduleResize();
}
