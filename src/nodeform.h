#pragma once
#include "flow.h"
#include "modelcatalog.h"
#include <QPlainTextEdit>
#include <QWidget>
#include <functional>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QCheckBox;
class QFormLayout;

namespace an {
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
    friend struct NodeRegistry;
    TextPreview *addPreview(QVBoxLayout *, const QString &label, const QString &field, const QString &text, const QString &placeholder);
    void buildAgent(QVBoxLayout *, QFormLayout *);
    void buildScript(QVBoxLayout *, QFormLayout *);
    void buildShell(QVBoxLayout *, QFormLayout *);
    void buildHost(QVBoxLayout *, QFormLayout *);
    void buildDelay(QVBoxLayout *, QFormLayout *);
    void buildSchedule(QVBoxLayout *, QFormLayout *);
    void buildUserInput(QVBoxLayout *, QFormLayout *);
    void buildInputs(QVBoxLayout *, QFormLayout *);
    void buildBranch(QVBoxLayout *, QFormLayout *);
    std::function<void(const Node &)> syncParameters_;
    std::function<void(bool)> readOnlyParameters_;
    std::function<void()> refreshFlowChoices_;
    void refreshTargetFlows();
    bool readOnly_ = false;
    Node node_;
    bool updating_ = false, resizeScheduled_ = false;
    QLineEdit *title_ = nullptr;
    QComboBox *agent_ = nullptr, *model_ = nullptr, *effort_ = nullptr;
    QCheckBox *retainContext_ = nullptr;
    QComboBox *scriptLanguage_ = nullptr;
    QLineEdit *scriptPath_ = nullptr, *scriptDirectory_ = nullptr;
    QPushButton *scriptFileBrowse_ = nullptr, *scriptDirectoryBrowse_ = nullptr;
    QLineEdit *shellDirectory_ = nullptr;
    QComboBox *shellOutput_ = nullptr;
    QComboBox *targetFlow_ = nullptr;
    QLineEdit *projectPath_ = nullptr;
    std::vector<std::pair<QString, QString>> flowChoices_;
    QPushButton *shellDirectoryBrowse_ = nullptr;
    QPointer<ModelCatalog> catalog_;
    QLabel *modelStatus_ = nullptr;
    QFormLayout *parameterForm_ = nullptr;
    QPushButton *refreshModels_ = nullptr;
    QSpinBox *inputs_ = nullptr;
    QSpinBox *waitDuration_ = nullptr;
    QComboBox *waitUnit_ = nullptr;
    QLineEdit *scheduleMonth_ = nullptr, *scheduleDay_ = nullptr, *scheduleHour_ = nullptr, *scheduleMinute_ = nullptr;
    std::vector<QCheckBox *> scheduleWeekdays_;
    std::vector<QLabel *> weekdayLabels_;
    void updateWeekdayLabels();
    TextPreview *prompt_ = nullptr, *schema_ = nullptr;
    TextPreview *shellCommand_ = nullptr;
    QWidget *rules_ = nullptr;
    QWidget *choices_ = nullptr;
    QVBoxLayout *choicesLayout_ = nullptr;
    QPushButton *addChoice_ = nullptr;
    QVBoxLayout *rulesLayout_ = nullptr;
    void publish();
    void populateModels();
    void populateEfforts();
    void rebuildRules();
    void rebuildChoices();
    void scheduleResize();
};
}
