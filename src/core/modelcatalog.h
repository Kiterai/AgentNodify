#pragma once
#include "shared/constants.h"
#include "agents/definition.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QPointer>

namespace an {
enum class CatalogState { Idle, Loading, Ready, Error };
struct CatalogEntry {
    CatalogState state = CatalogState::Idle;
    std::vector<ModelInfo> models;
    QString error;
    QDateTime lastAttempt, lastSuccess;
};

// Catalog-only protocols. No prompts, inference, auto-login, or tool approval.
class ModelCatalog : public QObject {
    Q_OBJECT
public:
    explicit ModelCatalog(QObject *parent = nullptr, int timeoutMs = limits::CatalogTimeoutMilliseconds);
    ~ModelCatalog() override;
    void setConfig(const AgentConfig &config);
    CatalogEntry entry(const QString &agent) const;
    void request(const QString &agent, bool force = false);
    void refreshAll(bool force = false);
signals:
    void changed(QString agent);
private:
    struct Job {
        QProcess *process;
        QTimer *timer;
        QByteArray buffer;
        qsizetype bytes = 0;
        std::unique_ptr<AgentCatalogProtocol> protocol;
    };
    AgentConfig config_;
    bool configured_ = false;
    int timeoutMs_;
    std::map<QString, CatalogEntry> entries_;
    std::map<QString, std::unique_ptr<Job>> jobs_;
    QList<QPointer<QProcess>> processes_;
    QString program(const QString &agent, const AgentConfig &config) const;
    void write(const QString &agent, const Json &message);
    void read(const QString &agent);
    void handle(const QString &agent, const QByteArray &line);
    void exited(const QString &agent, int code, QProcess::ExitStatus status);
    void apply(const QString &agent, const CatalogAction &action);
    void finish(const QString &agent, const QString &error = {});
    void stop(const QString &agent);
};
}
