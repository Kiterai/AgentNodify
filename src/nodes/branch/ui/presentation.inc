NodePresentation branchPresentation() {
    NodePresentation d(Kind::Branch);
    d.description = strings::BranchDescription;
    d.background = colors::BranchBackground;
    d.foreground = colors::BranchForeground;
    d.accent = colors::BranchAccent;
    d.specialBackground = true;
    d.compact = true;
    d.portCaption = [](const Node &n, bool input, unsigned index) {
        return input                     ? QString(strings::InputLabel)
               : index == n.rules.size() ? QString(strings::ElseLabel)
                                         : QString(strings::IndexedRuleLabel).arg(index + 1);
    };
    d.createForm = [](NodeForm &owner, QVBoxLayout *layout, QFormLayout *, ModelCatalog *) {
        return std::make_unique<BranchForm>(owner, layout);
    };
    return d;
}
