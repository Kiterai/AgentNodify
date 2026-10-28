NodePresentation flowWaitPresentation() {
    NodePresentation d(Kind::FlowWait);
    d.description = strings::FlowWaitDescription;
    d.createForm = [](NodeForm &owner, QVBoxLayout *, QFormLayout *fields, ModelCatalog *) {
        return std::make_unique<HostForm>(owner, fields);
    };
    return d;
}
