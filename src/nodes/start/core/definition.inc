NodeDefinition NodeRegistry::start() {
    NodeDefinition d;
    d.kind = Kind::Start;
    d.key = strings::Start;
    d.name = strings::StartName;
    d.source = true;
    d.inputs = [](const Node &) { return 0u; };
    d.inputPolicy = InputPolicy::Source;
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) -> Json {
        return {{strings::Type, strings::Object}, {strings::Properties, Json::object()}, {strings::AdditionalProperties, false}};
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeImmediate;
    return d;
}
