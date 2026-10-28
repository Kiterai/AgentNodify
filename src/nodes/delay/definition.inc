NodeDefinition NodeRegistry::delay() {
    NodeDefinition d;
    d.kind = Kind::Delay;
    d.key = strings::Delay;
    d.name = strings::Waiting;
    d.description = strings::DelayDescription;
    d.background = colors::TimerBackground;
    d.foreground = colors::TimerForeground;
    d.accent = colors::TimerAccent;
    d.specialBackground = true;
    d.named = false;
    d.compact = true;
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        try {
            waitMilliseconds(n);
        } catch (const std::exception &e) {
            errors << QString::fromUtf8(e.what());
        }
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::launchWait;
    d.planWait = &Runner::planDelay;
    d.portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
    return d;
}
