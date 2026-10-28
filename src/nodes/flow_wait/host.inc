void Coordinator::operateFlowWait(Runner *caller, const Node &node, const QString &, const QString &identity,
                                  Runner::HostCompletion complete) {
    const auto failed = [&](const QString &reason) { failOperation(node, reason, complete); };
    auto project =
        std::find_if(projects.begin(), projects.end(), [&](const Project &p) { return key(p.flowPath, p.directory) == identity; });
    if (project == projects.end()) {
        failed(strings::ProjectNotFound);
        return;
    }
    auto found = sessions_.find(identity);
    if (found == sessions_.end()) {
        failed(strings::ExecutionNotStarted);
        return;
    }
    const auto session = found->second;
    if (!session->active) {
        complete(session->output, session->success ? QString() : session->error);
        return;
    }
    std::set<QString> seen;
    if (!callers_.contains(caller) || reaches(identity, callers_.at(caller), seen)) {
        failed(strings::WaitCycle);
        return;
    }
    session->waiters.push_back({caller, std::move(complete)});
}
