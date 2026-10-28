NodeDefinition NodeRegistry::branch() {
    NodeDefinition d;
    d.kind = Kind::Branch;
    d.key = strings::Branch;
    d.name = strings::BranchName;
    d.named = false;
    d.editableOutputs = true;
    d.outputConnectionsRequired = true;
    d.trailingOutput = true;
    d.unconnectedOutputMessage = strings::UnconnectedBranchOutput;
    d.editableOutputCount = [](const Node &n) { return unsigned(n.rules.size()); };
    d.outputs = [](const Node &n) { return unsigned(n.rules.size()) + 1u; };
    d.sameOutput = [](const Node &a, unsigned i, const Node &b, unsigned j) {
        return a.rules[i].variable == b.rules[j].variable && a.rules[i].value == b.rules[j].value && a.rules[i].equals == b.rules[j].equals;
    };
    d.initialize = [](Node &n) { n.rules.push_back({strings::Result, strings::OkValue, true}); };
    d.variables = [](const Node &n) {
        QStringList paths;
        for (const auto &r : n.rules)
            paths << r.variable;
        return paths;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeBranch;
    return d;
}
