void Runner::prepareScript(Id id, const Json &input, ProcessJob &j, ProcessLaunch &launch) {
    const auto &n = flow_.nodes.at(id);
    auto &program = launch.program;
    auto &workingDirectory = launch.workingDirectory;
    auto &args = launch.args;
    auto &standardInput = launch.standardInput;
    j.eventsOutput = false;
    if (n.scriptLanguage == strings::PythonLanguage) {
        launch.environment.insert(strings::PYTHONUTF8, strings::EnvironmentEnabled);
        launch.environment.insert(strings::PYTHONIOENCODING, strings::Utf8);
    }
    const QFileInfo script(QDir(directory_).absoluteFilePath(n.scriptPath));
    if (!script.isFile() || !script.isReadable())
        throw std::runtime_error((strings::ReadScriptFailed + script.absoluteFilePath()).toStdString());
    const QFileInfo work(n.scriptDirectory.trimmed().isEmpty() ? script.canonicalPath()
                                                               : QDir(directory_).absoluteFilePath(n.scriptDirectory));
    if (!work.isDir())
        throw std::runtime_error((strings::MissingScriptDirectory + work.absoluteFilePath()).toStdString());
    workingDirectory = work.canonicalFilePath();
    activeExecutions_.at(id).directory = workingDirectory;
    emit executionChanged(activeExecutions_.at(id));
    if (!active_)
        return;
    if (n.scriptLanguage == strings::PythonLanguage) {
#ifdef Q_OS_WIN
        program = resolveExecutable(strings::PythonLanguage);
        if (program.isEmpty())
            program = resolveExecutable(strings::Python3);
#else
        program = resolveExecutable(strings::Python3);
        if (program.isEmpty())
            program = resolveExecutable(strings::PythonLanguage);
#endif
        if (program.isEmpty())
            throw std::runtime_error(strings::PythonNotFound);
        args << strings::PythonUnbufferedArgument << script.canonicalFilePath();
    } else {
        program = resolveExecutable(strings::Node);
        if (program.isEmpty())
            throw std::runtime_error(strings::NodeNotFound);
        args << script.canonicalFilePath();
    }
    standardInput = QByteArray::fromStdString(input.dump()) + '\n';
}
Json Runner::decodeScript(Id id, ProcessJob &j, int) {
    Json output;
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString text = decoder.decode(j.stdoutData);
    if (decoder.hasError())
        throw std::runtime_error(strings::ScriptOutputMustBeUtf8);
    output = Json::parse(j.stdoutData.toStdString(), nullptr, false);
    const bool structured = !output.is_discarded();
    if (!structured)
        output = text.toUtf8().toStdString();
    activeExecutions_.at(id).structuredOutput = structured;

    return output;
}
