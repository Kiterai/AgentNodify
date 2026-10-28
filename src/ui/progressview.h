#pragma once
#include "shared/constants.h"
#include "core/progress.h"
#include <QScrollArea>
#include <QWidget>

class QTreeWidget;
class QTextBrowser;
class QVBoxLayout;

namespace an {
// Shared Markdown / JSON presentation for messages, tool details and node outputs.
class ContentView : public QWidget {
    Q_OBJECT
public:
    explicit ContentView(QWidget *parent = nullptr);
    void setValue(const Json &value, bool markdown = true, bool forceJson = false);
    void setPlainText(const QString &text);
    QString toPlainText() const { return plain_; }
    void setCompactMessage(int maxLines = metrics::UserMessageLines);
private:
    QTextBrowser *browser_;
    QTreeWidget *tree_;
    QString plain_;
    Json value_;
    bool initialized_ = false, markdown_ = true, forceJson_ = false;
};

class ProgressView : public QScrollArea {
    Q_OBJECT
public:
    explicit ProgressView(QWidget *parent = nullptr);
    void setProgress(const QString &log, const QString &executionKey, State state, const Json *finalOutput = nullptr, bool structuredOutput = false, const QString &directory = {}, bool plainLog = false);
    QString toPlainText() const { return raw_; }
    bool finalIsJson() const;
private:
    class Card;
    QString key_, raw_;
    ProgressParser parser_;
    QVBoxLayout *layout_;
    QMap<QString, Card *> cards_;
    quint64 revision_ = 0;
    bool followBottom_ = true, adjustingScroll_ = false;
    void reset();
};
}
