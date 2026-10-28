NodePresentation allPresentation() {
    NodePresentation d(Kind::All);
    d.description = strings::AllDescription;
    d.background = colors::JoinBackground;
    d.accent = colors::JoinAccent;
    d.specialBackground = true;
    d.compact = true;
    d.portCaption = [](const Node &, bool input, unsigned index) {
        return input ? QString(strings::IndexedInputLabel).arg(index) : QString(strings::OutputLabel);
    };
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<InputPortsForm>(owner, fields);
    };
    return d;
}
