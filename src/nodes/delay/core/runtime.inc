void Runner::planDelay(const Node &node, PendingWait &wait, QString &message) {
    wait.duration = waitMilliseconds(node);
    wait.elapsed.start();
    message = QString(strings::DelayLogMessage).arg(node.waitDuration).arg(node.waitUnit);
}
