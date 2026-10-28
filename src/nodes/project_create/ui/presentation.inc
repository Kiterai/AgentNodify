NodePresentation projectCreatePresentation() {
    NodePresentation d(Kind::ProjectCreate);
    d.description = strings::ProjectCreateDescription;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<HostForm>(owner, fields);
    };
    return d;
}
