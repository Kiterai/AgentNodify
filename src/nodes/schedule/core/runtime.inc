void Runner::planSchedule(const Node &node, PendingWait &wait, QString &message) {
    wait.target = Schedule(node).next(now());
    message = strings::ScheduleTargetLogMessage + wait.target.toString(Qt::ISODateWithMs) + strings::Newline;
}
