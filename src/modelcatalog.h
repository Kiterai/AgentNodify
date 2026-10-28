#pragma once
#include "constants.h"
#include "runner.h"
#include <QDateTime>
#include <QPointer>
#include <QSet>

namespace an {
struct ModelInfo {
    QString id, name, description;
    QStringList efforts;
    bool operator==(const ModelInfo &) const = default;
};
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
        int requestId = 1;
        bool initialized = false;
        std::vector<ModelInfo> models;
        QSet<QString> seenCursors;
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
    void addModels(Job &job, const Json &models, const QString &agent);
    void finish(const QString &agent, const QString &error = {});
    void stop(const QString &agent);
};
}
