#pragma once
#include <QObject>
namespace an {
class CatalogTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void protocols_data();
    void protocols();
    void failures_data();
    void failures();
    void cacheAndRefresh();
    void pathChange();
    void emptyList();
    void paginationLoop();
    void manualValuePreserved();
};
}
