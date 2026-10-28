NodePresentation userInputPresentation() {
    NodePresentation d(Kind::UserInput);
    d.description = strings::UserInputDescription;
    d.background = colors::UserInputBackground;
    d.foreground = colors::UserInputForeground;
    d.accent = colors::UserInputAccent;
    d.specialBackground = true;
    d.compact = true;
    d.portCaption = [](const Node &n, bool input, unsigned index) {
        return input ? QString(strings::InputLabel) : n.userChoices.value(qsizetype(index));
    };
    d.createForm = [](NodeForm &owner, QVBoxLayout *layout, QFormLayout *, ModelCatalog *) {
        return std::make_unique<UserInputForm>(owner, layout);
    };
    return d;
}
