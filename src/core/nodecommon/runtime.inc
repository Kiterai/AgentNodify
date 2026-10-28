void Runner::executeHost(Id id, const Json &input) {
    const auto &n = flow_.nodes.at(id);
    if (!hostHandler)
        throw std::runtime_error(strings::HostUnavailable);
    const auto path = interpolate(n.projectPath, input);
    pendingHost_.insert(id);
    const auto generation = generation_;
    QPointer<Runner> guard(this);
    hostHandler(this, id, n, path.trimmed().isEmpty() ? QString() : QDir(directory_).absoluteFilePath(path),
                [guard, generation, id](const Json &output, const QString &error) {
                    if (!guard)
                        return;
                    QTimer::singleShot(0, guard, [guard, generation, id, output, error] {
                        if (!guard || !guard->active_ || guard->generation_ != generation || !guard->pendingHost_.erase(id))
                            return;
                        if (!error.isEmpty())
                            guard->fail(id, error);
                        else {
                            guard->complete(id, output);
                            guard->schedulePump();
                        }
                    });
                });
}
