#include "window.h"
#include "instancelock.h"
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("AgentNodify"); app.setApplicationName("AgentNodify");
    const bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) QStandardPaths::setTestModeEnabled(true);
    // Keep the isolated smoke-test lock inside the build tree, not real user data.
    const auto dataDirectory = smoke ? QCoreApplication::applicationDirPath() + "/smoke-instance"
                                     : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(dataDirectory)) {
        QMessageBox::warning(nullptr, "AgentNodify", "起動確認用のディレクトリを作成できません。アプリを終了します。");
        return 1;
    }
    an::InstanceLock instance(QDir(dataDirectory).filePath("instance.lock"));
    if (!instance.acquire()) {
        const auto message = instance.error() == QLockFile::LockFailedError
            ? "AgentNodifyはすでに起動しています。タスクトレイのアイコンからウィンドウを表示してください。"
            : "起動確認用のロックを取得できません。アプリを終了します。";
        QMessageBox::warning(nullptr, "AgentNodify", message);
        return 1;
    }
    an::Window window(smoke); window.show();
    QObject::connect(&window, &an::Window::quitRequested, &app, &QCoreApplication::quit);
    if (smoke) QTimer::singleShot(150, &app, [&] { app.exit(window.smokeTest() ? 0 : 1); });
    return app.exec();
}
