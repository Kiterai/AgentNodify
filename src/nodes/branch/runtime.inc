void Runner::executeBranch(Id id, const Json &input) {
    const auto &n = flow_.nodes.at(id);
    int selected = int(n.rules.size());
    for (int i = 0; i < int(n.rules.size()); ++i) {
        auto &rule = n.rules[size_t(i)];
        if ((display(resolve(input, rule.variable)) == rule.value) == rule.equals) {
            selected = i;
            break;
        }
    }
    complete(id, input, selected);
}
