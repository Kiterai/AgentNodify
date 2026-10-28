NodeDefinition NodeRegistry::all() {
    // Older portable flows used "and" for the All node.
    NodeDefinition d;
    d.kind = Kind::All;
    d.key = strings::All;
    d.name = strings::AllName;
    d.aliases = {strings::And};
    d.named = false;
    d.inputPolicy = InputPolicy::All;
    d.inputs = [](const Node &n) { return n.inputs; };
    d.schema = [](const Node &, const SchemaInputs &in) { return Json{{strings::Type, strings::Array}, {strings::Items, in.ports}}; };
    d.validate = [](const Node &n) {
        QStringList errors;
        if (n.inputs == 0 || n.inputs > limits::InputPorts)
            errors << QString(strings::InvalidInputCount).arg(limits::InputPorts);
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeImmediate;
    return d;
}
