void Runner::prepareAgent(Id id, const Json &input, ProcessJob &j, ProcessLaunch &launch) {
    const auto &n = flow_.nodes.at(id);
    auto &program = launch.program;
    auto &args = launch.args;
    auto &standardInput = launch.standardInput;
    QString prompt = interpolate(n.prompt, input);
    const auto session = n.retainContext && agentSessions_.contains(id) ? agentSessions_.at(id) : QString();
    j.temp = std::make_unique<QTemporaryDir>();
    if (!j.temp->isValid())
        throw std::runtime_error(strings::CreateTemporaryDirectoryFailed);
    if (n.agent == strings::Codex) {
        program = config_.codex;
        j.finalPath = j.temp->filePath(strings::ResponseTxt);
        args << strings::Exec << strings::JsonArgument << strings::SkipGitCheckArgument << strings::LastMessageArgument << j.finalPath;
        args << strings::WorkingDirectoryArgument << directory_;
        args << strings::SandboxArgument << strings::WorkspaceWrite;
        if (!n.schema.trimmed().isEmpty()) {
            auto path = j.temp->filePath(strings::SchemaJson);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(n.schema.toUtf8()) < 0)
                throw std::runtime_error(strings::WriteSchemaFailed);
            args << strings::OutputSchemaArgument << path;
        }
        if (!n.model.isEmpty())
            args << strings::ModelArgument << n.model;
        if (!n.effort.isEmpty())
            args << strings::ConfigArgument
                 << (strings::ReasoningEffortAssignment + QString::fromStdString(Json(n.effort.toStdString()).dump()));
        // Exec options precede the resume subcommand (not all are accepted
        // as resume-local flags). Never use --last across parallel nodes.
        if (!session.isEmpty())
            args << strings::Resume << session;
        args << strings::Dash;
    } else if (n.agent == strings::ClaudeCode) {
        program = config_.claude;
        args << strings::PromptArgument << strings::VerboseArgument << strings::OutputFormatArgument << strings::StreamJson
             << strings::PartialMessagesArgument;
        args << strings::PermissionModeArgument << strings::AcceptEdits;
        if (!n.effort.isEmpty())
            args << strings::EffortArgument << n.effort;
        if (!n.model.isEmpty())
            args << strings::ModelArgument << n.model;
        if (!n.schema.trimmed().isEmpty())
            args << strings::JsonSchemaArgument << n.schema;
        if (!session.isEmpty())
            args << strings::ResumeArgument << session;
    } else {
        // A shared service owns tool execution and can outlive the launcher's cwd.
        // Keep the server and its tools in this project's process tree instead.
        program = config_.opencode;
        args << strings::Run << strings::FormatArgument << strings::JsonFormat << strings::StandaloneArgument;
        if (!n.effort.isEmpty() && n.model.isEmpty())
            throw std::runtime_error(strings::OpenCodeEffortRequiresModel);
        if (!n.model.isEmpty())
            args << strings::ModelArgument << (n.effort.isEmpty() ? n.model : n.model.section('#', 0, 0) + strings::WholeInput + n.effort);
        if (!session.isEmpty())
            args << strings::SessionArgument << session;
        // OpenCode CLI has no documented native output-schema flag.
        if (!n.schema.trimmed().isEmpty())
            prompt += strings::JsonSchemaPromptSuffix + n.schema;
        args << strings::EndOfOptions << prompt;
    }
    if (n.agent != strings::OpenCode)
        standardInput = prompt.toUtf8();
    // Encode as one record so prompt text cannot be parsed as CLI events.
    appendLog(id, QByteArray::fromStdString(
                      Json{{strings::Type, strings::AgentnodifyPrompt}, {strings::Prompt, prompt.toUtf8().toStdString()}}.dump()) +
                      '\n');
}
Json Runner::decodeAgent(Id id, ProcessJob &j, int) {
    const auto &n = flow_.nodes.at(id);
    Json output;
    if (n.agent == strings::Codex && QFileInfo::exists(j.finalPath)) {
        QFile file(j.finalPath);
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error(strings::ReadFinalResponseFailed);
        auto data = file.readAll();
        output = n.schema.trimmed().isEmpty() ? Json(data.toStdString()) : Json::parse(data.toStdString());
    } else
        output = decodeAgentOutput(n.agent, j.stdoutData, !n.schema.trimmed().isEmpty());
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
