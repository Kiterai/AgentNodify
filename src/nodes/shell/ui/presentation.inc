NodePresentation shellPresentation() {
    NodePresentation d(Kind::Shell);
    d.description = strings::ShellDescription;
    d.accent = colors::ShellAccent;
    d.textFields = {strings::ShellCommand};
    d.processOutput = true;
    d.createForm = [](NodeForm &owner, QVBoxLayout *layout, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<ShellForm>(owner, layout, fields);
    };
    return d;
}
