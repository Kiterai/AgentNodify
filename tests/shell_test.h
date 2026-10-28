#pragma once
#include <QObject>
namespace an {
class ShellTest : public QObject {
    Q_OBJECT
private slots:
    void definition();
    void result_data();
    void result();
    void directories_data();
    void directories();
    void savedFlowContext();
    void variablesAndNonzeroExit();
    void textForwarding();
    void loop();
    void parallelJoin();
    void failures_data();
    void failures();
    void outputLimits_data();
    void outputLimits();
    void example();
    void shellDiagnostics();
};
}
