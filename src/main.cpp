#include "window.h"
#include <QApplication>
#include <QTimer>
#include <QStandardPaths>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("AgentNodify"); app.setApplicationName("AgentNodify");
    const bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) QStandardPaths::setTestModeEnabled(true);
    an::Window window(smoke); window.show();
    QObject::connect(&window, &an::Window::quitRequested, &app, &QCoreApplication::quit);
    if (smoke) QTimer::singleShot(150, &app, [&] { app.exit(window.smokeTest() ? 0 : 1); });
    return app.exec();
}
