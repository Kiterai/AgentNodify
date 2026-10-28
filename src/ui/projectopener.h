#pragma once
#include <QStringList>
#include <vector>

namespace an {
enum class ProjectApp { Explorer, CommandPrompt, PowerShell, GitBash, Code, CodeInsiders };
struct ProjectOpener {
    ProjectApp app;
    QString label, program;
    QStringList arguments(const QString &directory) const;
};
std::vector<ProjectOpener> detectProjectOpeners();
bool openProjectDirectory(const ProjectOpener &opener, const QString &directory, QString *error);
}
