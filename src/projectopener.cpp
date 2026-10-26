#include "projectopener.h"
#include "cli.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace an {
QStringList ProjectOpener::arguments(const QString &directory) const {
    const auto path = QDir::toNativeSeparators(QFileInfo(directory).absoluteFilePath());
    switch (app) {
    case ProjectApp::Explorer: return {path};
    case ProjectApp::CommandPrompt: return {"/D", "/K"};
    case ProjectApp::PowerShell: return {"-NoLogo", "-NoProfile", "-NoExit"};
    case ProjectApp::GitBash: return {"--cd=" + path};
    default: return {"--new-window", path};
    }
}
std::vector<ProjectOpener> detectProjectOpeners() {
    std::vector<ProjectOpener> result;
#ifdef Q_OS_WIN
    const auto local = qEnvironmentVariable("LOCALAPPDATA");
    const auto windows = qEnvironmentVariable("WINDIR");
    const QStringList roots{qEnvironmentVariable("ProgramW6432"), qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)"), local + "/Programs"};
    auto add = [&](ProjectApp app, const QString &label, const QStringList &names, QStringList candidates) {
        for (const auto &name : names) {
            candidates << resolveExecutable(name);
            for (const auto &hive : {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"}) {
                QSettings key(QString(hive) + "\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + name, QSettings::NativeFormat);
                candidates << key.value(".").toString();
            }
        }
        for (auto path : candidates) {
            if (path.startsWith('"') && path.endsWith('"')) path = path.mid(1, path.size() - 2);
            QFileInfo info(path);
            if (info.isAbsolute() && info.isFile() && info.suffix().compare("exe", Qt::CaseInsensitive) == 0) {
                result.push_back({app, label, info.canonicalFilePath()}); return;
            }
        }
    };
    add(ProjectApp::Explorer, "エクスプローラーで開く", {"explorer.exe"}, {windows + "/explorer.exe"});
    add(ProjectApp::CommandPrompt, "コマンドプロンプトで開く", {"cmd.exe"}, {windows + "/System32/cmd.exe"});
    QStringList powershell, git, code, insiders;
    for (const auto &root : roots) if (!root.isEmpty()) {
        powershell << root + "/PowerShell/7/pwsh.exe";
        git << root + "/Git/git-bash.exe";
        code << root + "/Microsoft VS Code/Code.exe";
        insiders << root + "/Microsoft VS Code Insiders/Code - Insiders.exe";
    }
    // Prefer modern PowerShell, with Windows PowerShell as a fallback.
    powershell.prepend(resolveExecutable("pwsh.exe"));
    powershell << windows + "/System32/WindowsPowerShell/v1.0/powershell.exe";
    for (const auto &hive : {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"}) {
        QSettings key(QString(hive) + "\\Software\\GitForWindows", QSettings::NativeFormat);
        const auto install = key.value("InstallPath").toString();
        if (!install.isEmpty()) git << install + "/git-bash.exe";
    }
    const auto gitExe = resolveExecutable("git.exe");
    if (!gitExe.isEmpty()) git << QFileInfo(gitExe).dir().filePath("../git-bash.exe") << QFileInfo(gitExe).dir().filePath("../../git-bash.exe");
    // VS Code normally puts only its bin/code*.cmd launchers on PATH.
    // Resolve their native sibling rather than executing a shell wrapper.
    for (const auto &alias : {"code.cmd", "code"}) {
        const auto command = resolveExecutable(alias);
        if (!command.isEmpty()) code << QFileInfo(command).dir().filePath("../Code.exe");
    }
    for (const auto &alias : {"code-insiders.cmd", "code-insiders"}) {
        const auto command = resolveExecutable(alias);
        if (!command.isEmpty()) insiders << QFileInfo(command).dir().filePath("../Code - Insiders.exe");
    }
    add(ProjectApp::PowerShell, "PowerShellで開く", {"pwsh.exe", "powershell.exe"}, powershell);
    add(ProjectApp::GitBash, "Git Bashで開く", {"git-bash.exe"}, git);
    add(ProjectApp::Code, "VS Codeで開く", {"Code.exe"}, code);
    add(ProjectApp::CodeInsiders, "VS Code Insidersで開く", {"Code - Insiders.exe"}, insiders);
#endif
    return result;
}
bool openProjectDirectory(const ProjectOpener &opener, const QString &directory, QString *error) {
    if (!QFileInfo(directory).isDir() || !QFileInfo(opener.program).isFile()) {
        if (error) *error = "ディレクトリまたはアプリが見つかりません。"; return false;
    }
    QProcess process;
    process.setProgram(opener.program); process.setArguments(opener.arguments(directory));
    const auto path = QFileInfo(directory).canonicalFilePath(); process.setWorkingDirectory(path);
    auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("PWD", path); process.setProcessEnvironment(environment);
#ifdef Q_OS_WIN
    if (opener.app == ProjectApp::CommandPrompt || opener.app == ProjectApp::PowerShell) {
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags = (args->flags & ~(DETACHED_PROCESS | CREATE_NO_WINDOW)) | CREATE_NEW_CONSOLE;
            args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
            args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            args->startupInfo->wShowWindow = SW_SHOWNORMAL;
        });
    }
#endif
    if (process.startDetached()) return true;
    if (error) *error = process.errorString(); return false;
}
}
