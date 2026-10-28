NodeDefinition NodeRegistry::agent() {
    NodeDefinition d;
    d.kind = Kind::Agent;
    d.key = strings::Agent;
    d.name = strings::AgentName;
    d.description = strings::AgentDescription;
    d.background = colors::NodeBackground;
    d.foreground = colors::NodeForeground;
    d.accent = colors::AgentAccent;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &n, const SchemaInputs &) {
        return n.schema.trimmed().isEmpty() ? Json{{strings::Type, strings::String}} : parseJson(n.schema);
    };
    d.requiresModel = true;
    d.interpolation = [](const Node &n) { return n.prompt; };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.prompt.trimmed().isEmpty())
            errors << strings::EmptyPrompt;
        if (n.agent != strings::Codex && n.agent != strings::ClaudeCode && n.agent != strings::OpenCode)
            errors << strings::UnknownAgent;
        try {
            if (!n.schema.trimmed().isEmpty())
                checkSchema(parseJson(n.schema));
        } catch (const std::exception &e) {
            errors << QString(strings::InvalidJsonSchema) + e.what();
        }
        return errors;
    };
    d.structured = [](const Node &n) { return !n.schema.trimmed().isEmpty(); };
    d.executionAgent = [](const Node &n) { return n.agent; };
    d.execute = &Runner::launch;
    d.prepare = &Runner::prepareAgent;
    d.decode = &Runner::decodeAgent;
    d.portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
    d.textFields = {strings::Prompt, strings::Schema};
    d.processOutput = false;
    return d;
}
