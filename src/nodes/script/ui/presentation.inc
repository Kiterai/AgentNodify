NodePresentation scriptPresentation() {
    NodePresentation d(Kind::Script);
    d.description = strings::ScriptDescription;
    d.accent = colors::ScriptAccent;
    d.processOutput = true;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<ScriptForm>(owner, fields);
    };
    return d;
}
