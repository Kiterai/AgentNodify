class ScheduleForm final : public NodeParameterForm {
public:
    ScheduleForm(NodeForm &owner, QFormLayout *form);
private:
    QLineEdit *scheduleMonth_ = nullptr, *scheduleDay_ = nullptr, *scheduleHour_ = nullptr, *scheduleMinute_ = nullptr;
    std::vector<QCheckBox *> scheduleWeekdays_;
    std::vector<QLabel *> weekdayLabels_;
    void updateWeekdayLabels();
};

ScheduleForm::ScheduleForm(NodeForm &owner, QFormLayout *form) : NodeParameterForm(owner) {
    const auto &node = node_;
    auto addField = [this, form](const QString &label, const QString &name, const QString &value, int low, int high, QString Node::*field) {
        auto *edit = new QLineEdit(value);
        edit->setObjectName(name);
        edit->setMinimumWidth(0);
        edit->setPlaceholderText(strings::AnyScheduleValuePlaceholder);
        edit->setToolTip(QString(strings::ScheduleRangeTooltip).arg(low).arg(high));
        form->addRow(label, edit);
        connect(edit, &QLineEdit::textEdited, this, [this, field](const QString &text) {
            if (!updating_ && !readOnly_) {
                node_.*field = text;
                publish();
            }
        });
        return edit;
    };
    scheduleMonth_ = addField(strings::MonthLabel, strings::ScheduleMonth, node.scheduleMonth, calendar::FirstMonth, calendar::LastMonth,
                              &Node::scheduleMonth);
    scheduleDay_ =
        addField(strings::DayLabel, strings::ScheduleDay, node.scheduleDay, calendar::FirstDay, calendar::LastDay, &Node::scheduleDay);
    scheduleHour_ = addField(strings::HourLabel, strings::ScheduleHour, node.scheduleHour, calendar::FirstHour, calendar::LastHour,
                             &Node::scheduleHour);
    scheduleMinute_ = addField(strings::MinuteLabel, strings::ScheduleMinute, node.scheduleMinute, calendar::FirstMinute,
                               calendar::LastMinute, &Node::scheduleMinute);
    auto *weekdays = new QWidget;
    weekdays->setObjectName(strings::ScheduleWeekdays);
    weekdays->setFixedWidth(metrics::WeekdayTableWidth);
    auto *grid = new QGridLayout(weekdays);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(metrics::SpaceHairline);
    grid->setVerticalSpacing(metrics::SpaceTiny);
    const QStringList names = {strings::MonthLabel,  strings::TuesdayShort,  strings::WednesdayShort, strings::ThursdayShort,
                               strings::FridayShort, strings::SaturdayShort, strings::DayLabel};
    for (int i = 0; i < calendar::LastWeekday; ++i) {
        auto *check = new NodeCheckBox(strings::Empty);
        check->setFixedSize(metrics::WeekdayWidth, metrics::WeekdayHeight);
        check->setObjectName(QString(strings::ScheduleWeekdayObjectName).arg(i + 1));
        check->setAccessibleName(names[i] + strings::WeekdayLabel);
        check->setChecked(std::find(node.scheduleWeekdays.begin(), node.scheduleWeekdays.end(), i + 1) != node.scheduleWeekdays.end());
        auto *label = new QLabel(names[i]);
        label->setFixedWidth(metrics::WeekdayWidth);
        label->setObjectName(QString(strings::ScheduleWeekdayLabelObjectName).arg(i + 1));
        label->setAlignment(Qt::AlignCenter);
        grid->addWidget(check, 0, i, Qt::AlignCenter);
        grid->addWidget(label, 1, i);
        scheduleWeekdays_.push_back(check);
        weekdayLabels_.push_back(label);
        connect(check, &QCheckBox::toggled, this, [this] {
            if (updating_ || readOnly_)
                return;
            node_.scheduleWeekdays.clear();
            for (size_t day = 0; day < scheduleWeekdays_.size(); ++day)
                if (scheduleWeekdays_[day]->isChecked())
                    node_.scheduleWeekdays.push_back(int(day + 1));
            updateWeekdayLabels();
            publish();
        });
    }
    form->addRow(strings::WeekdayLabel, weekdays);
    updateWeekdayLabels();
    syncParameters_ = [this](const Node &) {
        const auto &node = node_;
        for (const auto &[edit, value] : std::vector<std::pair<QLineEdit *, QString>>{{scheduleMonth_, node.scheduleMonth},
                                                                                      {scheduleDay_, node.scheduleDay},
                                                                                      {scheduleHour_, node.scheduleHour},
                                                                                      {scheduleMinute_, node.scheduleMinute}})
            if (edit && edit->text() != value)
                edit->setText(value);
        for (size_t i = 0; i < scheduleWeekdays_.size(); ++i)
            scheduleWeekdays_[i]->setChecked(std::find(node.scheduleWeekdays.begin(), node.scheduleWeekdays.end(), int(i + 1)) !=
                                             node.scheduleWeekdays.end());
        updateWeekdayLabels();
    };
    readOnlyParameters_ = [this](bool readOnly) {
        for (auto *check : scheduleWeekdays_)
            check->setEnabled(!readOnly);
    };
}

void ScheduleForm::updateWeekdayLabels() {
    for (size_t i = 0; i < weekdayLabels_.size(); ++i)
        weekdayLabels_[i]->setStyleSheet(scheduleWeekdays_[i]->isChecked() ? strings::ActiveWeekdayStyle : strings::InactiveWeekdayStyle);
}
