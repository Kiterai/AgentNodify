#pragma once
#include "shared/constants.h"
#include "graph.h"
#include "core/coordinator.h"
#include "progressview.h"
#include <QMainWindow>
#include <QListWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QPushButton>
#include <QToolButton>
#include <QUuid>
#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QUndoStack>
#include <memory>

class QSystemTrayIcon;
class QComboBox;
class QHBoxLayout;

namespace an {
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(bool smoke = false);
    ~Window() override;
    bool smokeTest();
signals:
    void quitRequested();
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    friend class GuiTest;
    void removeFlow(int index);
    struct Document {
        Flow flow; QString path; bool dirty = false;
        std::optional<QPointF> creationAnchor;
        bool offsetCreation = false;
        std::shared_ptr<QUndoStack> undo = std::make_shared<QUndoStack>();
        quint64 savedNextNodeId = 0;
    };
    struct Project {
        QString name, directory, flowPath;
        std::map<Id, Result> results;
        QString historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        std::vector<NodeExecution> history;
        std::optional<Flow> runFlow;
        std::map<Id, QPointF> workPositions;
    };
    std::vector<Document> documents_;
    std::vector<Project> projects_;
    int current_ = -1, project_ = -1;
    Id selected_ = 0, workSelected_ = 0;
    QString field_;
    QString dataDir_;
    bool loading_ = false, smoke_ = false;
    bool quitting_ = false;
    QSystemTrayIcon *trayIcon_ = nullptr;
    void setupTray();
    void showWindow();
    void requestQuit();
    AgentConfig config_;
    Graph *editGraph_, *workGraph_;
    Runner *runner_;
    Coordinator *coordinator_;
    bool busy() const;
    void updateExecutionUi();
    void updateTerminalButton();
    void syncCoordinatorRegistry();
    bool confirmStopProjects(const QStringList &ids);
    bool suppressRunWarnings_ = false;
    void refreshFlowChoices();
    ModelCatalog *modelCatalog_;
    QtNodes::BasicGraphicsScene *editScene_, *workScene_;
    QtNodes::GraphicsView *editView_, *workView_;
    QWidget *activities_;
    QVBoxLayout *activityLayout_;
    QListWidget *projectList_;
    QListWidget *historyList_;
    QToolButton *followHistoryButton_;
    QString selectedHistory_;
    bool followLatestHistory_ = true, historySaveFailed_ = false;
    QTimer *historySaveTimer_;
    std::map<QString, std::pair<QString, NodeExecution>> pendingHistory_;
    QTabWidget *tabs_;
    QSplitter *editorSplit_;
    QWidget *textPane_;
    QPlainTextEdit *text_;
    ProgressView *log_;
    ContentView *output_;
    QLabel *textTitle_, *textError_, *status_, *nodeStatus_;
    QPushButton *runButton_, *stopButton_;
    struct InputPrompt { QString projectId; UserInputRequest request; QString draft; };
    std::vector<InputPrompt> inputPrompts_;
    QString activeInput_;
    std::map<QString, QString> selectedInputs_;
    QSplitter *workInputSplit_ = nullptr;
    int inputPaneHeight_ = metrics::DefaultInputPaneHeight;
    QWidget *inputPanel_ = nullptr;
    QComboBox *inputSelector_ = nullptr;
    ContentView *inputMessage_ = nullptr;
    QPlainTextEdit *inputText_ = nullptr;
    QHBoxLayout *inputButtons_ = nullptr;
    QLabel *inputShortcutHint_ = nullptr;
    bool controlPressed_ = false;
    void updateInputShortcutHint();
    QPointF nextNodePosition() const;
    void refreshUserInputPanel();
    void focusUserInputNode();
    void buildUi();
    void refreshActivities();
    void reorderFlow(int from, int to);
    void reorderProject(int from, int to);
    QPoint reorderPress_;
    QString reorderSource_;
    void selectFlow(int index);
    void selectNode(Id id);
    void showText(const QString &field);
    void refreshProjects();
    void refreshProjectStatusIcons();
    void selectProject(int index);
    void selectWorkNode(Id id);
    void refreshWorkHistory();
    void showWorkResult();
    void recordNodeExecution(const QString &projectId, const NodeExecution &execution);
    void flushHistory();
    void newFlow();
    void openFlow();
    bool saveFlow(bool saveAs = false);
    void validateFlow();
    void addProject();
    void executeFlow();
    bool currentFlowRunning() const;
    void settings();
    void persist();
    void restore();
    void changed();
    QAction *undoAction_ = nullptr, *redoAction_ = nullptr;
    bool applyingHistory_ = false;
    int editBatch_ = 0;
    quint64 historyGroup_ = 0;
    void recordEdit(const Flow &before, const Flow &after);
    void updateUndoActions();
    void stepHistory(bool redo);
    void applyHistory(const Flow &flow);
    bool confirmNodeDeletion(int count);
    void deleteEditorObjects(std::optional<Id> node = {});
    void editorOperation(const std::function<void()> &operation);
    bool canPasteEditorObjects(bool fromClipboard);
    bool editorTarget(QWidget *widget) const;
    void reportError(const QString &message);
};
}
