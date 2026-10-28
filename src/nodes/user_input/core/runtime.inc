void Runner::executeUserInput(Id id, const Json &input) {
    const auto &n = flow_.nodes.at(id);
    UserInputRequest request{QUuid::createUuid().toString(QUuid::WithoutBraces), id, input, n.userChoices};
    pendingInput_[id] = {request, false};
    emit userInputRequested(request);
}
