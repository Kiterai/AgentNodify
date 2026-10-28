NodeDefinition NodeRegistry::any() {
    NodeDefinition d;
    d.kind = Kind::Any;
    d.key = strings::Any;
    d.name = strings::AnyName;
    d.named = false;
    d.inputPolicy = InputPolicy::Any;
    d.inputs = [](const Node &n) { return n.inputs; };
    d.schema = [](const Node &n, const SchemaInputs &in) {
        return Json{
            {strings::Type, strings::Object},
            {strings::Properties,
             {{strings::Event, {{strings::Type, strings::Integer}, {strings::Minimum, 0}, {strings::Maximum, n.inputs ? n.inputs - 1 : 0}}},
              {strings::Input, in.merged}}},
            {strings::Required, {strings::Event, strings::Input}},
            {strings::AdditionalProperties, false}};
    };
    d.validate = [](const Flow &, const Node &n) {
        QStringList errors;
        if (n.inputs == 0 || n.inputs > limits::InputPorts)
            errors << QString(strings::InvalidInputCount).arg(limits::InputPorts);
        return errors;
    };
    d.structured = [](const Node &) { return true; };
    d.execute = &Runner::executeImmediate;
    return d;
}
