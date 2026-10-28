NodeDefinition NodeRegistry::flowWait() {
    NodeDefinition d;
    d.kind = Kind::FlowWait;
    d.key = strings::FlowWait;
    d.name = strings::FlowWaitName;
    d.description = strings::FlowWaitDescription;
    d.background = colors::NodeBackground;
    d.foreground = colors::NodeForeground;
    d.accent = colors::HostAccent;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) { return Json{{strings::AnUnknown, true}}; };
    d.host = true;
    d.operate = &Coordinator::operateFlowWait;
    d.hostFailure = [](const QString &) { return Json(); };
    d.interpolation = [](const Node &n) { return n.projectPath; };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.targetFlow.isEmpty())
            errors << strings::MissingTargetFlow;
        if (n.projectPath.trimmed().isEmpty())
            errors << strings::EmptyProjectPath;
        return errors;
    };
    d.structured = [](const Node &) { return true; };
    d.execute = &Runner::executeHost;
    d.portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
    return d;
}
