#pragma once
#include <QObject>

class LayeringTest : public QObject {
    Q_OBJECT
  private slots:
    void coreRunsWithoutTheUi();
    void lowerLayersNeverIncludeTheUi();
};
