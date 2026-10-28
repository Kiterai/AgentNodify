NodeDefinition NodeRegistry::delay() {
    NodeDefinition d;
    d.kind = Kind::Delay;
    d.key = strings::Delay;
    d.name = strings::Waiting;
    d.named = false;
    d.validate = [](const Node &n) {
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
    return d;
}
