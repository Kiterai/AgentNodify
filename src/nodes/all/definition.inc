NodeDefinition NodeRegistry::all() {
    // Older portable flows used "and" for the All node.
    NodeDefinition d;
    d.kind = Kind::All;
    d.key = strings::All;
    d.name = strings::AllName;
    d.description = strings::AllDescription;
    d.aliases = {strings::And};
    d.background = colors::JoinBackground;
    d.foreground = colors::NodeForeground;
    d.accent = colors::JoinAccent;
    d.specialBackground = true;
    d.named = false;
    d.compact = true;
    d.inputPolicy = InputPolicy::All;
    d.inputs = [](const Node &n) { return n.inputs; };
    d.schema = [](const Node &, const SchemaInputs &in) { return Json{{strings::Type, strings::Array}, {strings::Items, in.ports}}; };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.inputs == 0 || n.inputs > limits::InputPorts)
            errors << QString(strings::InvalidInputCount).arg(limits::InputPorts);
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeImmediate;
    d.portCaption = [](const Node &, bool input, unsigned index) {
        return input ? QString(strings::IndexedInputLabel).arg(index) : QString(strings::OutputLabel);
    };
    return d;
}
