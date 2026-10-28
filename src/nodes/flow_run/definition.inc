NodeDefinition NodeRegistry::flowRun() {
    NodeDefinition d;
    d.kind = Kind::FlowRun;
    d.key = strings::FlowRun;
    d.name = strings::FlowRunName;
    d.description = strings::FlowRunDescription;
    d.background = colors::NodeBackground;
    d.foreground = colors::NodeForeground;
    d.accent = colors::HostAccent;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) -> Json {
        return {{strings::Type, strings::Object}, {strings::Properties, {{strings::Result, {{strings::Type, strings::String}}}}}};
    };
    d.host = true;
    d.operate = &Coordinator::operateFlowRun;
    d.hostFailure = [](const QString &) { return Json{{strings::Result, strings::Failed}}; };
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
