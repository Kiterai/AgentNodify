#include "app/instancelock.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <cstdio>

class InstanceTest : public QObject {
    Q_OBJECT
private slots:
    void rejectsSecondProcess() {
        QTemporaryDir directory; const auto path = directory.filePath("instance.lock");
        an::InstanceLock first(path); QVERIFY(first.acquire());
        QProcess second; second.start(QCoreApplication::applicationFilePath(), {"--probe", path});
        QVERIFY(second.waitForFinished(5000)); QCOMPARE(second.exitCode(), 2);
        // Rejection must not release or replace the original process's lock.
        an::InstanceLock third(path); QVERIFY(!third.acquire()); QCOMPARE(third.error(), QLockFile::LockFailedError);
    }
    void releasesOnNormalExit() {
        QTemporaryDir directory; const auto path = directory.filePath("instance.lock");
        { an::InstanceLock first(path); QVERIFY(first.acquire()); }
        an::InstanceLock next(path); QVERIFY(next.acquire());
    }
    void recoversAfterCrash() {
        QTemporaryDir directory; const auto path = directory.filePath("instance.lock");
        QProcess first; first.start(QCoreApplication::applicationFilePath(), {"--hold", path});
        QVERIFY(first.waitForReadyRead(5000)); QCOMPARE(first.readAllStandardOutput().trimmed(), QByteArray("locked"));
        an::InstanceLock second(path); QVERIFY(!second.acquire());
        first.kill(); QVERIFY(first.waitForFinished(5000));
        an::InstanceLock next(path); QVERIFY(next.acquire());
    }
    void reportsFileErrors() {
        QTemporaryDir directory; QFile file(directory.filePath("file")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        an::InstanceLock lock(directory.filePath("file/instance.lock")); QVERIFY(!lock.acquire()); QVERIFY(lock.error() != QLockFile::LockFailedError);
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    if (args.size() == 3 && (args[1] == "--probe" || args[1] == "--hold")) {
        an::InstanceLock instance(args[2]);
        if (!instance.acquire()) return instance.error() == QLockFile::LockFailedError ? 2 : 3;
        if (args[1] == "--probe") return 0;
        std::puts("locked"); std::fflush(stdout);
        QTimer::singleShot(30000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    InstanceTest test; return QTest::qExec(&test, argc, argv);
}
#include "instance_test.moc"
