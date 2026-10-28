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
class QDoubleSpinBox;

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
    QDoubleSpinBox *waitDuration_ = nullptr;
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
