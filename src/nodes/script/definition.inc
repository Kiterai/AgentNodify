NodeDefinition NodeRegistry::script() {
    NodeDefinition d;
    d.kind = Kind::Script;
    d.key = strings::Script;
    d.name = strings::ScriptName;
    d.description = strings::ScriptDescription;
    d.background = colors::NodeBackground;
    d.foreground = colors::NodeForeground;
    d.accent = colors::ScriptAccent;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) { return Json{{strings::AnUnknown, true}}; };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.scriptLanguage != strings::PythonLanguage && n.scriptLanguage != strings::JavaScriptLanguage)
            errors << strings::InvalidScriptLanguage;
        if (n.scriptPath.trimmed().isEmpty())
            errors << strings::EmptyScriptPath;
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.executionAgent = [](const Node &n) {
        return n.scriptLanguage == strings::PythonLanguage ? QString(strings::Python) : QString(strings::NodeJs);
    };
    d.execute = &Runner::launch;
    d.prepare = &Runner::prepareScript;
    d.decode = &Runner::decodeScript;
    d.portCaption = [](const Node &, bool input, unsigned) { return input ? QString(strings::InputLabel) : QString(strings::OutputLabel); };
    d.textFields = {};
    d.processOutput = true;
    return d;
}
