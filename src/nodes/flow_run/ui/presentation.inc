NodePresentation flowRunPresentation() {
    NodePresentation d(Kind::FlowRun);
    d.description = strings::FlowRunDescription;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<HostForm>(owner, fields);
    };
    return d;
}
