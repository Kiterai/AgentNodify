#include "shared/strings.h"
#include "projectopener.h"
#include "core/cli.h"
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
    case ProjectApp::Finder: return {path};
    case ProjectApp::Terminal: return {strings::OpenApplicationArgument, strings::TerminalApplication, path};
    case ProjectApp::Explorer: return {path};
    case ProjectApp::CommandPrompt: return {strings::CmdDisableAutoRunArgument, strings::CmdKeepOpenArgument};
    case ProjectApp::PowerShell: return {strings::NoLogoArgument, strings::NoProfileArgument, strings::PowerShellKeepOpenArgument};
    case ProjectApp::GitBash: return {strings::BashDirectoryArgument + path};
    default: return {strings::EditorNewWindowArgument, path};
    }
}
std::vector<ProjectOpener> detectProjectOpeners() {
    std::vector<ProjectOpener> result;
#ifdef Q_OS_WIN
    const auto local = qEnvironmentVariable(strings::LOCALAPPDATA);
    const auto windows = qEnvironmentVariable(strings::WINDIR);
    const QStringList roots{qEnvironmentVariable(strings::ProgramW6432), qEnvironmentVariable(strings::ProgramFiles), qEnvironmentVariable(strings::ProgramFilesX86Environment), local + strings::LocalProgramsSuffix};
    auto add = [&](ProjectApp app, const QString &label, const QStringList &names, QStringList candidates) {
        for (const auto &name : names) {
            candidates << resolveExecutable(name);
            for (const auto &hive : {strings::HKEYCURRENTUSER, strings::HKEYLOCALMACHINE}) {
                QSettings key(QString(hive) + strings::ApplicationPathsRegistry + name, QSettings::NativeFormat);
                candidates << key.value(strings::Dot).toString();
            }
        }
        for (auto path : candidates) {
            if (path.startsWith('"') && path.endsWith('"')) path = path.mid(1, path.size() - 2);
            QFileInfo info(path);
            if (info.isAbsolute() && info.isFile() && info.suffix().compare(strings::Exe, Qt::CaseInsensitive) == 0) {
                result.push_back({app, label, info.canonicalFilePath()}); return;
            }
        }
    };
    add(ProjectApp::Explorer, strings::ExplorerAction, {strings::ExplorerExe}, {windows + strings::ExplorerExecutableSuffix});
    add(ProjectApp::CommandPrompt, strings::CommandPromptAction, {strings::CmdExe}, {windows + strings::CmdExecutableSuffix});
    QStringList powershell, git, code, insiders;
    for (const auto &root : roots) if (!root.isEmpty()) {
        powershell << root + strings::PowerShellCoreSuffix;
        git << root + strings::GitBashSuffix;
        code << root + strings::VsCodeSuffix;
        insiders << root + strings::VsCodeInsidersSuffix;
    }
    // Prefer modern PowerShell, with Windows PowerShell as a fallback.
    powershell.prepend(resolveExecutable(strings::PwshExe));
    powershell << windows + strings::WindowsPowerShellSuffix;
    for (const auto &hive : {strings::HKEYCURRENTUSER, strings::HKEYLOCALMACHINE}) {
        QSettings key(QString(hive) + strings::GitRegistry, QSettings::NativeFormat);
        const auto install = key.value(strings::InstallPath).toString();
        if (!install.isEmpty()) git << install + strings::GitBashRootSuffix;
    }
    const auto gitExe = resolveExecutable(strings::GitExe);
    if (!gitExe.isEmpty()) git << QFileInfo(gitExe).dir().filePath(strings::GitBashParentPath) << QFileInfo(gitExe).dir().filePath(strings::GitBashAncestorPath);
    // VS Code normally puts only its bin/code*.cmd launchers on PATH.
    // Resolve their native sibling rather than executing a shell wrapper.
    for (const auto &alias : {strings::CodeCmd, strings::Code}) {
        const auto command = resolveExecutable(alias);
        if (!command.isEmpty()) code << QFileInfo(command).dir().filePath(strings::VsCodeParentPath);
    }
    for (const auto &alias : {strings::CodeInsidersCmd, strings::CodeInsiders}) {
        const auto command = resolveExecutable(alias);
        if (!command.isEmpty()) insiders << QFileInfo(command).dir().filePath(strings::VsCodeInsidersParentPath);
    }
    add(ProjectApp::PowerShell, strings::PowerShellAction, {strings::PwshExe, strings::PowershellExe}, powershell);
    add(ProjectApp::GitBash, strings::GitBashAction, {strings::GitBashExe}, git);
    add(ProjectApp::Code, strings::VsCodeAction, {strings::CodeExe}, code);
    add(ProjectApp::CodeInsiders, strings::VsCodeInsidersAction, {strings::CodeInsidersExe}, insiders);
#elif defined(Q_OS_MACOS)
    // macOS launches its file manager and terminal via Launch Services, not a
    // shell command. Keep this dependency inside the project-opening adapter.
    if (QFileInfo(strings::MacOpenExecutable).isExecutable()) {
        result.push_back({ProjectApp::Finder, strings::FinderAction, strings::MacOpenExecutable});
        result.push_back({ProjectApp::Terminal, strings::TerminalAction, strings::MacOpenExecutable});
    }
    for (const auto app : {ProjectApp::Code, ProjectApp::CodeInsiders}) {
        const auto program = resolveExecutable(app == ProjectApp::Code ? strings::Code : strings::CodeInsiders);
        if (!program.isEmpty()) result.push_back({app, app == ProjectApp::Code ? strings::VsCodeAction : strings::VsCodeInsidersAction, program});
    }
#endif
    return result;
}
bool openProjectDirectory(const ProjectOpener &opener, const QString &directory, QString *error) {
    if (!QFileInfo(directory).isDir() || !QFileInfo(opener.program).isFile()) {
        if (error) *error = strings::ProjectApplicationNotFound; return false;
    }
    QProcess process;
    process.setProgram(opener.program); process.setArguments(opener.arguments(directory));
    const auto path = QFileInfo(directory).canonicalFilePath(); process.setWorkingDirectory(path);
    auto environment = QProcessEnvironment::systemEnvironment(); environment.insert(strings::PWD, path); process.setProcessEnvironment(environment);
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
