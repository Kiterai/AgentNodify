NodeDefinition NodeRegistry::end() {
    NodeDefinition d;
    d.kind = Kind::End;
    d.key = strings::End;
    d.name = strings::EndName;
    d.terminal = true;
    d.outputs = [](const Node &) { return 0u; };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeImmediate;
    return d;
}
