void NodeForm::buildDelay(QVBoxLayout *, QFormLayout *form) {
    const auto &node = node_;
    waitDuration_ = new QSpinBox;
    waitDuration_->setObjectName(strings::WaitDuration);
    waitDuration_->setMinimumWidth(0);
    waitDuration_->setRange(0, limits::WaitDuration);
    waitDuration_->setValue(node.waitDuration);
    waitUnit_ = new QComboBox;
    waitUnit_->setObjectName(strings::WaitUnit);
    styleComboBoxPopup(waitUnit_);
    for (const auto &[label, value] : std::vector<std::pair<QString, QString>>{{strings::DayLabel, strings::Days},
                                                                               {strings::HoursLabel, strings::Hours},
                                                                               {strings::MinuteLabel, strings::Minutes},
                                                                               {strings::SecondLabel, strings::Seconds}})
        waitUnit_->addItem(label, value);
    if (waitUnit_->findData(node.waitUnit) < 0)
        waitUnit_->addItem(node.waitUnit + strings::UnsupportedSuffix, node.waitUnit);
    waitUnit_->setCurrentIndex(waitUnit_->findData(node.waitUnit));
    form->addRow(strings::WaitDurationLabel, waitDuration_);
    form->addRow(strings::UnitLabel, waitUnit_);
    connect(waitDuration_, &QSpinBox::valueChanged, this, [this](int value) {
        if (!updating_ && !readOnly_) {
            node_.waitDuration = value;
            publish();
        }
    });
    connect(waitUnit_, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_ && !readOnly_) {
            node_.waitUnit = waitUnit_->currentData().toString();
            publish();
        }
    });
    syncParameters_ = [this](const Node &) {
        const auto &node = node_;
        if (waitDuration_)
            waitDuration_->setValue(node.waitDuration);
        if (waitUnit_) {
            if (waitUnit_->findData(node.waitUnit) < 0)
                waitUnit_->addItem(node.waitUnit + strings::UnsupportedSuffix, node.waitUnit);
            waitUnit_->setCurrentIndex(waitUnit_->findData(node.waitUnit));
        }
    };
}
