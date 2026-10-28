#include "shared/constants.h"
#include "shared/strings.h"
#include "ui/window.h"
#include "instancelock.h"
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName(an::strings::ApplicationName); app.setApplicationName(an::strings::ApplicationName);
    const bool smoke = app.arguments().contains(an::strings::SmokeArgument);
    if (smoke) QStandardPaths::setTestModeEnabled(true);
    // Keep the isolated smoke-test lock inside the build tree, not real user data.
    const auto dataDirectory = smoke ? QCoreApplication::applicationDirPath() + an::strings::SmokeInstanceDirectory
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
    an::Window window(smoke); window.show();
    QObject::connect(&window, &an::Window::quitRequested, &app, &QCoreApplication::quit);
    if (smoke) QTimer::singleShot(an::time::SmokeMilliseconds, &app, [&] { app.exit(window.smokeTest() ? 0 : 1); });
    return app.exec();
}
