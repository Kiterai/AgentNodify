#pragma once
#include "shared/strings.h"
#include <QString>
#include <QMap>

namespace an {
// Preserve existing fields/aggregate initialization and workspace keys.
struct AgentConfig {
    QString codex = strings::CodexExecutable;
    QString claude = strings::ClaudeExecutable;
#ifdef Q_OS_WIN
    QString opencode = strings::OpenCodeWindowsExecutable;
#else
    QString opencode = strings::OpenCodeExecutable;
#endif
    bool opencodeStandalone = false;
    QMap<QString, QString> additionalExecutables;
};
}
