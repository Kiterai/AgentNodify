#pragma once
#include <nlohmann/json.hpp>
#include <QProcess>
#include <QMap>
#include <QElapsedTimer>
#include <QDateTime>
#include <QTimer>
#include <QTemporaryDir>
#include <QStringDecoder>
#include <memory>
namespace an {
// Shared process transport. Node orchestration lives in nodes/; agent CLI
// preparation and protocol decoding are supplied by core/agents/registry.cpp.
struct ProcessJob {
    QProcess *process = nullptr;
    QByteArray stdoutData, stderrData;
    bool eventsOutput = true, captureStderr = false, allowNonzeroExit = false;
    QStringDecoder stdoutDecoder{QStringDecoder::Utf8}, stderrDecoder{QStringDecoder::Utf8};
    QString finalPath;
    std::unique_ptr<QTemporaryDir> temp;
};
// Both time-based nodes use this cancellable timer state.
struct PendingWait {
    QTimer *timer = nullptr;
    nlohmann::json input;
    QElapsedTimer elapsed;
    qint64 duration = 0;
    QDateTime target;
};
struct ProcessLaunch {
    QString program, workingDirectory;
    QStringList args;
    QByteArray standardInput;
    QMap<QString, QString> environment;
};
} // namespace an
