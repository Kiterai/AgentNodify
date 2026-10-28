void Coordinator::operateProjectCreate(Runner *, const Node &node, const QString &directory, const QString &identity,
                                       Runner::HostCompletion complete) {
    const auto failed = [&](const QString &reason) { failOperation(node, reason, complete); };
    auto project =
        std::find_if(projects.begin(), projects.end(), [&](const Project &p) { return key(p.flowPath, p.directory) == identity; });
    if (project != projects.end()) {
        failed(strings::ProjectAlreadyExists);
        return;
    }
    if (QFileInfo::exists(directory) && !QFileInfo(directory).isDir()) {
        failed(strings::PathNotDirectory);
        return;
    }
    if (!QDir().mkpath(directory)) {
        failed(strings::DirectoryCreateFailed);
        return;
    }
    Project created{QUuid::createUuid().toString(QUuid::WithoutBraces), node.targetFlow, normalizedPath(directory)};
    projects.push_back(created);
    emit projectCreated(created.id, created.flowPath, created.directory);
    complete(Json{{strings::Result, strings::Success}, {strings::PathValue, created.directory.toStdString()}}, {});
    return;
}
