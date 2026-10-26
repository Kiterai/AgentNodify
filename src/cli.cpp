#include "cli.h"
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace an {
QString resolveExecutable(const QString &program) {
    if (QFileInfo(program).isAbsolute()) return program;
    auto resolved = QStandardPaths::findExecutable(program);
#ifdef Q_OS_WIN
    if (resolved.isEmpty()) {
        QStringList paths;
        for (auto key : {"HKEY_CURRENT_USER\\Environment", "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment"}) {
            QSettings settings(QString::fromLatin1(key), QSettings::NativeFormat);
            auto path = settings.value("Path").toString();
            QRegularExpression variable("%([^%]+)%");
            auto matches = variable.globalMatch(path); QString expanded; qsizetype last = 0;
            while (matches.hasNext()) {
                auto match = matches.next(); expanded += path.mid(last, match.capturedStart() - last);
                expanded += QProcessEnvironment::systemEnvironment().value(match.captured(1), match.captured());
                last = match.capturedEnd();
            }
            expanded += path.mid(last); paths += expanded.split(';', Qt::SkipEmptyParts);
        }
        resolved = QStandardPaths::findExecutable(program, paths);
    }
#endif
    return resolved;
}
}
