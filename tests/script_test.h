#pragma once
#include <QObject>
namespace an {
class ScriptTest : public QObject {
    Q_OBJECT
private slots:
    void definitionAndValidation();
    void execution_data();
    void execution();
    void outputTypes_data();
    void outputTypes();
    void failures_data();
    void failures();
    void loop_data();
    void loop();
    void parallelJoin();
    void outputLimit();
    void example();
};
}
