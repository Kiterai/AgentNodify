NodeDefinition NodeRegistry::projectCreate() {
    NodeDefinition d;
    d.kind = Kind::ProjectCreate;
    d.key = strings::ProjectCreate;
    d.name = strings::ProjectCreateName;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) -> Json {
        return {{strings::Type, strings::Object},
                {strings::Properties,
                 {{strings::Result, {{strings::Type, strings::String}}},
                  {strings::PathValue, {{strings::Type, strings::String}}},
                  {strings::Reason, {{strings::Type, strings::String}}}}}};
    };
    d.host = true;
    d.operate = &Coordinator::operateProjectCreate;
    d.hostFailure = [](const QString &reason) { return Json{{strings::Result, strings::Failed}, {strings::Reason, reason.toStdString()}}; };
    d.interpolation = [](const Node &n) { return n.projectPath; };
    d.validate = [](const Node &n) {
        QStringList errors;
        if (n.targetFlow.isEmpty())
            errors << strings::MissingTargetFlow;
        if (n.projectPath.trimmed().isEmpty())
            errors << strings::EmptyProjectPath;
        return errors;
    };
    d.structured = [](const Node &) { return true; };
    d.execute = &Runner::executeHost;
    return d;
}
