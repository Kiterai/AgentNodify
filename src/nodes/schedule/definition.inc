NodeDefinition NodeRegistry::schedule() {
    NodeDefinition d;
    d.kind = Kind::Schedule;
    d.key = strings::Schedule;
    d.name = strings::ScheduleName;
    d.description = strings::ScheduleDescription;
    d.background = colors::TimerBackground;
    d.foreground = colors::TimerForeground;
    d.accent = colors::TimerAccent;
    d.specialBackground = true;
    d.named = false;
    d.compact = true;
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        try {
            Schedule(n).next(
                QDateTime(QDate(calendar::ValidationYear, calendar::FirstMonth, calendar::FirstDay), QTime(0, 0), QTimeZone::UTC));
        } catch (const std::exception &e) {
            errors << QString::fromUtf8(e.what());
        }
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::launchWait;
    d.planWait = &Runner::planSchedule;
    d.portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
    return d;
}
