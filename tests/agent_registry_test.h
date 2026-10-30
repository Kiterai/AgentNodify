#pragma once
#include <QObject>

class AgentRegistryTest : public QObject {
    Q_OBJECT
private slots:
    void registrationIsComplete();
    void configurationRemainsCompatible();
    void launchUsesRegisteredImplementation();
    void catalogProtocolInstancesAreIndependent();
    void progressStateAndResetRemainLocal();
    void implementationBoundariesStayLocal();
};
