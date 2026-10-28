NodeDefinition NodeRegistry::schedule() {
    NodeDefinition d;
    d.kind = Kind::Schedule;
    d.key = strings::Schedule;
    d.name = strings::ScheduleName;
    d.named = false;
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
    return d;
}
