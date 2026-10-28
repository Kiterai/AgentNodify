void Runner::prepareShell(Id id, const Json &input, ProcessJob &j, ProcessLaunch &launch) {
    const auto &n = flow_.nodes.at(id);
    auto &program = launch.program;
    auto &workingDirectory = launch.workingDirectory;
    auto &args = launch.args;
    j.eventsOutput = false;
    j.captureStderr = true;
    j.allowNonzeroExit = true;
    if ((n.shellDirectory.trimmed().isEmpty() || QDir::isRelativePath(n.shellDirectory)) && flowDirectory_.isEmpty())
        throw std::runtime_error(strings::MissingFlowDirectory);
    const QFileInfo work(n.shellDirectory.trimmed().isEmpty() ? flowDirectory_ : QDir(flowDirectory_).absoluteFilePath(n.shellDirectory));
    if (!work.isDir())
        throw std::runtime_error((strings::MissingShellDirectory + work.absoluteFilePath()).toStdString());
    workingDirectory = work.canonicalFilePath();
    const auto command = interpolate(n.shellCommand, input);
#ifdef Q_OS_WIN
    program = resolveExecutable(strings::Powershell);
    if (program.isEmpty())
        program = resolveExecutable(strings::Pwsh);
    if (program.isEmpty())
        throw std::runtime_error(strings::PowerShellNotFound);
    const auto source = strings::PowerShellUtf8Preamble + command;
    args << strings::NoLogoArgument << strings::NoProfileArgument << strings::NonInteractiveArgument
         << strings::PowerShellOutputFormatArgument << strings::Text << strings::PowerShellCommandArgument << source;
#else
    program = strings::PosixShellExecutable;
    args << strings::ConfigArgument << command;
#endif
    activeExecutions_.at(id).directory = workingDirectory;
    emit executionChanged(activeExecutions_.at(id));
    if (!active_)
        return;
}
Json Runner::decodeShell(Id id, ProcessJob &j, int code) {
    const auto &n = flow_.nodes.at(id);
    Json output;
    if (n.shellOutput == strings::TextFormat)
        output = QString::fromUtf8(j.stdoutData).toStdString();
    else
        output = {{strings::ExitCode, code},
                  {strings::Stdout, QString::fromUtf8(j.stdoutData).toStdString()},
                  {strings::Stderr, QString::fromUtf8(j.stderrData).toStdString()}};
    return output;
}
