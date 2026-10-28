NodePresentation agentPresentation() {
    NodePresentation d(Kind::Agent);
    d.description = strings::AgentDescription;
    d.accent = colors::AgentAccent;
    d.textFields = {strings::Prompt, strings::Schema};
    d.createForm = [](NodeForm &owner, QVBoxLayout *layout, QFormLayout *fields, ModelCatalog *catalog) {
        return std::make_unique<AgentForm>(owner, layout, fields, catalog);
    };
    return d;
}
