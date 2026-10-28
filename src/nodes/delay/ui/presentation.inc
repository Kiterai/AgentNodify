NodePresentation delayPresentation() {
    NodePresentation d(Kind::Delay);
    d.description = strings::DelayDescription;
    d.background = colors::TimerBackground;
    d.foreground = colors::TimerForeground;
    d.accent = colors::TimerAccent;
    d.specialBackground = true;
    d.compact = true;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<DelayForm>(owner, fields);
    };
    return d;
}
