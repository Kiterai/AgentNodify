#include "shared/constants.h"
#include "shared/strings.h"
#include "ui/window.h"
#include "instancelock.h"
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QTemporaryDir>
#include <optional>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName(an::strings::ApplicationName); app.setApplicationName(an::strings::ApplicationName);
    const bool smoke = app.arguments().contains(an::strings::SmokeArgument);
    if (smoke) QStandardPaths::setTestModeEnabled(true);
    // Installed/signed bundles may be read-only. Smoke tests never write into
    // the application or real user data (TMPDIR can select the build tree).
    std::optional<QTemporaryDir> smokeData;
    if (smoke) smokeData.emplace();
    const auto dataDirectory = smoke ? smokeData->path()
                                     : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(dataDirectory)) {
        QMessageBox::warning(nullptr, an::strings::ApplicationName, an::strings::CreateInstanceDirectoryFailed);
        return 1;
    }
    an::InstanceLock instance(QDir(dataDirectory).filePath(an::strings::InstanceLock));
    if (!instance.acquire()) {
        const auto message = instance.error() == QLockFile::LockFailedError
            ? an::strings::AlreadyRunning
            : an::strings::AcquireInstanceLockFailed;
        QMessageBox::warning(nullptr, an::strings::ApplicationName, message);
        return 1;
    }
    an::Window window(smoke);
    app.setWindowIcon(window.windowIcon());
    window.showMaximized();
    QObject::connect(&window, &an::Window::quitRequested, &app, &QCoreApplication::quit);
    if (smoke) QTimer::singleShot(an::time::SmokeMilliseconds, &app, [&] { app.exit(window.smokeTest() ? 0 : 1); });
    return app.exec();
}
