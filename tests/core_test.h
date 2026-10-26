#pragma once
#include "flow.h"
#include "runner.h"
#include <QObject>
class CoreTest : public QObject {
    Q_OBJECT
    an::Flow simple(const QString &agent = "Codex");
    QString schema();
    an::AgentConfig config();
private slots:
    void variables();
    void schemas();
    void roundtrip();
    void invalidFlows();
    void schemaReferences();
    void protocols();
    void cli_data();
    void cli();
    void branchAndJoin();
    void parallelJoin();
    void inactiveAgent();
    void examples();
    void loopExecution();
    void repeatedAnd();
    void queuedActivations();
    void incompleteAnd();
    void synchronousLoopCancellation();
    void loopValidation();
    void failuresAndCancellation();
    void rejectBeforeExecution();
};
