void Runner::prepareAgent(Id id, const Json &input, ProcessJob &j, ProcessLaunch &launch) {
    const auto &n = flow_.nodes.at(id);
    const auto session = n.retainContext && agentSessions_.contains(id) ? agentSessions_.at(id) : QString();
    const auto prompt = prepareAgentLaunch({n, config_, directory_, interpolate(n.prompt, input), session}, j, launch);
    // Encode as one record so prompt text cannot be parsed as CLI events.
    appendLog(id, QByteArray::fromStdString(
                      Json{{strings::Type, strings::AgentnodifyPrompt}, {strings::Prompt, prompt.toUtf8().toStdString()}}.dump()) +
                      '\n');
}
Json Runner::decodeAgent(Id id, ProcessJob &j, int) {
    const auto &n = flow_.nodes.at(id);
    auto output = readAgentResponse(n.agent, j, !n.schema.trimmed().isEmpty());
    if (!n.schema.trimmed().isEmpty())
        validateOutput(parseJson(n.schema), output);
    if (n.retainContext) {
        const auto session = decodeAgentSessionId(n.agent, j.stdoutData);
        if (session.isEmpty())
            throw std::runtime_error(strings::MissingSessionId);
        if (agentSessions_.contains(id) && agentSessions_.at(id) != session)
            throw std::runtime_error(strings::SessionMismatch);
        agentSessions_[id] = session;
    }
    return output;
}
