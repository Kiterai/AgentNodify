NodeDefinition NodeRegistry::shell() {
    NodeDefinition d;
    d.kind = Kind::Shell;
    d.key = strings::Shell;
    d.name = strings::ShellName;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &n, const SchemaInputs &) {
        return n.shellOutput == strings::TextFormat ? Json{{strings::Type, strings::String}}
                                                    : Json{{strings::Type, strings::Object},
                                                           {strings::Properties,
                                                            {{strings::ExitCode, {{strings::Type, strings::Integer}}},
                                                             {strings::Stdout, {{strings::Type, strings::String}}},
                                                             {strings::Stderr, {{strings::Type, strings::String}}}}},
                                                           {strings::Required, {strings::ExitCode, strings::Stdout, strings::Stderr}},
                                                           {strings::AdditionalProperties, false}};
    };
    d.interpolation = [](const Node &n) { return n.shellCommand; };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.shellCommand.trimmed().isEmpty())
            errors << strings::EmptyShellCommand;
        if (n.shellOutput != strings::JsonFormat && n.shellOutput != strings::TextFormat)
            errors << strings::InvalidShellOutput;
        return errors;
    };
    d.structured = [](const Node &n) { return n.shellOutput == strings::JsonFormat; };
    d.executionAgent = [](const Node &) {
#ifdef Q_OS_WIN
        return QString(strings::PowerShell);
#else
        return QString(strings::Sh);
#endif
    };
    d.execute = &Runner::launch;
    d.prepare = &Runner::prepareShell;
    d.decode = &Runner::decodeShell;
    return d;
}
