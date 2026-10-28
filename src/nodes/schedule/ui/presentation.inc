NodePresentation schedulePresentation() {
    NodePresentation d(Kind::Schedule);
    d.description = strings::ScheduleDescription;
    d.background = colors::TimerBackground;
    d.foreground = colors::TimerForeground;
    d.accent = colors::TimerAccent;
    d.specialBackground = true;
    d.compact = true;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<ScheduleForm>(owner, fields);
    };
    return d;
}
