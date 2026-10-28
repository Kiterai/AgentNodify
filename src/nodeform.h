#pragma once
#include "flow.h"
#include <QPlainTextEdit>
#include <QWidget>
#include <functional>
#include <memory>

class QLineEdit;
class QVBoxLayout;

namespace an {
class ModelCatalog;
class NodeParameterForm;
QString nodeDescription(Kind kind);

// A node-local preview; editing is delegated to the full-size right pane.
class TextPreview : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit TextPreview(QWidget *parent = nullptr);
    int visibleLines() const { return lines_; }
signals:
    void editRequested();
    void heightChanged();
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    int lines_ = 1;
    void resizeToText();
};

class NodeForm : public QWidget {
    Q_OBJECT
public:
    explicit NodeForm(const Node &node, QWidget *parent = nullptr, ModelCatalog *catalog = nullptr);
    ~NodeForm() override;
    Node node() const { return node_; }
    std::function<bool()> beforeEdit;
    void setReadOnly(bool readOnly);
    void setNode(const Node &node);
    void setFlowChoices(const std::vector<std::pair<QString, QString>> &choices);
signals:
    void edited();
    void outputRemoved(unsigned index);
    void activated();
    void textEditRequested(QString field);
    void sizeChanged();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    bool event(QEvent *event) override;
private:
    friend class NodeParameterForm;
    TextPreview *addPreview(QVBoxLayout *, const QString &label, const QString &field, const QString &text, const QString &placeholder);
    bool readOnly_ = false;
    Node node_;
    bool updating_ = false, resizeScheduled_ = false;
    QLineEdit *title_ = nullptr;
    std::unique_ptr<NodeParameterForm> parameters_;
    void publish();
    void scheduleResize();
};
}
