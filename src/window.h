#pragma once
#include "graph.h"
#include <QMainWindow>
#include <QListWidget>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QSplitter>
#include <QPushButton>
#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>

namespace an {
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(bool smoke = false);
    bool smokeTest();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    struct Document { Flow flow; QString path; bool dirty = false; };
    struct Project { QString name, directory, flowPath; std::map<Id, Result> results; };
    std::vector<Document> documents_;
    std::vector<Project> projects_;
    int current_ = -1, project_ = -1;
    Id selected_ = 0, workSelected_ = 0;
    QString field_;
    QString dataDir_;
    bool loading_ = false, smoke_ = false;
    AgentConfig config_;
    Graph *editGraph_, *workGraph_;
    Runner *runner_;
    QtNodes::BasicGraphicsScene *editScene_, *workScene_;
    QtNodes::GraphicsView *editView_, *workView_;
    QListWidget *activities_, *projectList_;
    QTabWidget *tabs_;
    QSplitter *editorSplit_;
    QWidget *parameters_, *textPane_;
    QPlainTextEdit *text_, *log_, *output_;
    QLabel *textTitle_, *textError_, *status_, *nodeStatus_;
    QPushButton *runButton_, *stopButton_;
    void buildUi();
    void refreshActivities();
    void selectFlow(int index);
    void selectNode(Id id);
    void showText(const QString &field);
    void refreshProjects();
    void selectProject(int index);
    void selectWorkNode(Id id);
    void newFlow();
    void openFlow();
    bool saveFlow(bool saveAs = false);
    void validateFlow();
    void addProject();
    void executeFlow();
    void settings();
    void editRules();
    void connectNodes();
    void persist();
    void restore();
    void changed();
    void reportError(const QString &message);
};
}
