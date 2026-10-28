void Coordinator::operateFlowRun(Runner *, const Node &node, const QString &, const QString &identity, Runner::HostCompletion complete) {
    const auto failed = [&](const QString &reason) { failOperation(node, reason, complete); };
    auto project =
        std::find_if(projects.begin(), projects.end(), [&](const Project &p) { return key(p.flowPath, p.directory) == identity; });
    if (project == projects.end()) {
        failed(strings::ProjectNotFound);
        return;
    }
    if (sessions_.contains(identity) && sessions_.at(identity)->active) {
        complete(Json{{strings::Result, strings::Already}}, {});
        return;
    }
    // Copy: launching can indirectly grow the project registry.
    const auto target = *project;
    complete(Json{{strings::Result, start(target) ? strings::Started : strings::Failed}}, {});
    return;
}
