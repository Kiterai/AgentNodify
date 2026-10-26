#include "window.h"
#include "theme.h"
#include "graphview.h"
#include "projectopener.h"
#include "history.h"
#include <algorithm>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QToolButton>
#include <QTextCharFormat>
#include <QUuid>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QStyle>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

namespace an {
namespace {
class Highlighter : public QSyntaxHighlighter {
public:
    explicit Highlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
    void highlightBlock(const QString &line) override {
        const std::pair<QString, QString> rules[] = {
            {R"("(?:[^"\\]|\\.)*")", "#9cd6ad"},
            {R"(\b(?:true|false|null|\d+)\b)", "#e9ba7c"},
            {R"(\$\{[^}]+\})", "#8fbdff"}
        };
        for (auto &[pattern, color] : rules) {
            QTextCharFormat format; format.setForeground(QColor(color));
            auto it = QRegularExpression(pattern).globalMatch(line);
            while (it.hasNext()) { auto m = it.next(); setFormat(int(m.capturedStart()), int(m.capturedLength()), format); }
        }
    }
};
QPushButton *button(const QString &text, QLayout *layout, std::function<void()> action) {
    auto *b = new QPushButton(text); layout->addWidget(b); QObject::connect(b, &QPushButton::clicked, b, std::move(action)); return b;
}
QString uniquePath(const QString &dir) { return QDir(dir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".flow.json"); }
std::string str(const QString &s) { return s.toUtf8().toStdString(); }
QString qs(const Json &j) { return QString::fromUtf8(j.get<std::string>()); }
QStringList modelSelectionErrors(const Flow &flow) {
    QStringList errors;
    for (const auto &[id, node] : flow.nodes) if (node.kind == Kind::Agent && node.model.isEmpty())
        errors << "モデルを選択してください: " + (node.title.isEmpty() ? kindName(node.kind) : node.title);
    return errors;
}
}
Window::Window(bool smoke) : smoke_(smoke) {
    setWindowTitle("AgentNodify"); resize(1440, 900);
    dataDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir_ + "/flows");
    editGraph_ = new Graph; editGraph_->setParent(this);
    workGraph_ = new Graph; workGraph_->setParent(this); workGraph_->readOnly = true;
    runner_ = new Runner(this);
    modelCatalog_ = new ModelCatalog(this);
    if (!smoke_) editGraph_->modelCatalog = workGraph_->modelCatalog = modelCatalog_;
    buildUi();
    historySaveTimer_ = new QTimer(this); historySaveTimer_->setSingleShot(true); historySaveTimer_->setInterval(1000);
    connect(historySaveTimer_, &QTimer::timeout, this, &Window::flushHistory);
    connect(runner_, &Runner::executionChanged, this, &Window::recordNodeExecution);
    connect(editGraph_, &Graph::edited, this, [this] { changed(); });
    connect(runner_, &Runner::changed, this, [this](Id id) {
        if (project_ < 0 || !runner_->results().contains(id)) return;
        projects_[size_t(project_)].results[id] = runner_->results().at(id);
        workGraph_->states[id] = runner_->results().at(id).state; emit workGraph_->nodeUpdated(id);
        if (workSelected_ == id) showWorkResult();
    });
    connect(runner_, &Runner::finished, this, [this](bool ok, const QString &message) {
        flushHistory();
        runButton_->setEnabled(true); stopButton_->setEnabled(false);
        activities_->setEnabled(true); projectList_->setEnabled(true); tabs_->setTabEnabled(0, true);
        status_->setText(message); statusBar()->showMessage(message, 15000);
        if (!ok && !smoke_) QMessageBox::warning(this, "実行結果", message);
    });
    auto *animation = new QTimer(this); animation->setInterval(180);
    connect(animation, &QTimer::timeout, this, [this] {
        ++workGraph_->frame;
        for (auto &[id, state] : workGraph_->states) if (state == State::Running) emit workGraph_->nodeUpdated(id);
    }); animation->start();
    if (!smoke_) restore();
    if (!smoke_) modelCatalog_->setConfig(config_);
    if (documents_.empty()) newFlow(); else { refreshActivities(); selectFlow(0); }
    if (!smoke_) QTimer::singleShot(0, modelCatalog_, [this] { modelCatalog_->refreshAll(); });
}
void Window::buildUi() {
    auto *file = menuBar()->addMenu("ファイル");
    file->setObjectName("fileMenu");
    file->addAction("新しいフロー", QKeySequence::New, this, [this] { newFlow(); });
    file->addAction("開く…", QKeySequence::Open, this, [this] { openFlow(); });
    file->addAction("保存", QKeySequence::Save, this, [this] { saveFlow(); });
    file->addAction("名前を付けて保存…", QKeySequence::SaveAs, this, [this] { saveFlow(true); });
    file->addSeparator(); file->addAction("終了", QKeySequence::Quit, this, &QWidget::close);
    auto *tools = menuBar()->addMenu("ツール");
    tools->addAction("フローを検証", QKeySequence("F7"), this, [this] { validateFlow(); });
    tools->addAction("CLI設定…", this, [this] { settings(); });
    tools->addAction("モデル一覧を更新", this, [this] { if (!smoke_) modelCatalog_->refreshAll(true); });
    auto *help = menuBar()->addMenu("ヘルプ");
    help->addAction("操作方法", this, [this] {
        QMessageBox::information(this, "操作方法", "左のボタンでノードを追加し、丸いポートをドラッグして接続します。\nパラメータはノード内のフォームで編集します。複数行欄をクリックすると右ペインで編集できます。\nノードや追加ボタンにカーソルを置くと機能説明を表示します。Deleteで選択要素を削除できます。\nAND入力は0-indexed、分岐は上から順・最後が「それ以外」です。\nポート数を変えると、そのノードの接続は解除されます。\nフロー一覧の右クリックで名前変更・一覧から削除、＋から新規作成・既存追加ができます。\nフローを保存し、プロジェクト作業タブでディレクトリを追加して実行します。\n作業ノードをクリックすると右ペインに状態・CLIログ・出力を表示します。\nCLIの認証と権限設定は各CLIで事前に行ってください。");
    });
    auto *root = new QWidget; auto *row = new QHBoxLayout(root); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0);
    auto *bar = new QWidget; bar->setFixedWidth(88); auto *barLayout = new QVBoxLayout(bar); barLayout->setContentsMargins(2, 6, 2, 6);
    auto *flowScroll = new QScrollArea; flowScroll->setObjectName("flowScroll"); flowScroll->setWidgetResizable(true);
    flowScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    activities_ = new QWidget; activities_->setObjectName("flowRail"); activities_->setMinimumWidth(70);
    activityLayout_ = new QVBoxLayout(activities_); activityLayout_->setContentsMargins(6, 10, 6, 10); activityLayout_->setSpacing(12);
    flowScroll->setWidget(activities_); barLayout->addWidget(flowScroll, 1); row->addWidget(bar);
    tabs_ = new QTabWidget; tabs_->setObjectName("mainTabs"); row->addWidget(tabs_, 1);
    editorSplit_ = new QSplitter;
    auto *left = new QWidget; auto *leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new QLabel("ノードを追加"));
    for (Kind k : {Kind::Start, Kind::Agent, Kind::And, Kind::Branch, Kind::End}) {
        auto *add = button("＋ " + kindName(k), leftLayout, [this, k] {
            auto id = editGraph_->addNode(kindKey(k));
            if (id == QtNodes::InvalidNodeId) statusBar()->showMessage("始端・終端はそれぞれ1つだけです", 5000);
            else selectNode(id);
        });
        add->setToolTip(nodeDescription(k)); add->setObjectName("addNode_" + kindKey(k));
    }
    button("フローを検証", leftLayout, [this] { validateFlow(); });
    leftLayout->addStretch(); left->setObjectName("nodePalette");
    left->setMinimumWidth(180); editorSplit_->addWidget(left);
    editScene_ = new QtNodes::BasicGraphicsScene(*editGraph_, this); editView_ = new GraphView(editScene_);
    editView_->setScaleRange(0.15, 3); editorSplit_->addWidget(editView_);
    editView_->setObjectName("flowEditorView");
    connect(editScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectNode(id); });
    connect(editScene_, &QtNodes::BasicGraphicsScene::nodeContextMenu, this, [this](Id id, const QPointF &point) {
        if (runner_->running() || !editGraph_->nodeExists(id)) return;
        QMenu menu(this); menu.setObjectName("nodeContextMenu");
        auto *remove = menu.addAction("削除");
        const auto kind = editGraph_->flow.nodes.at(id).kind;
        remove->setEnabled(kind != Kind::Start && kind != Kind::End);
        connect(remove, &QAction::triggered, this, [this, id] { if (!runner_->running()) editGraph_->deleteNode(id); });
        menu.exec(editView_->viewport()->mapToGlobal(editView_->mapFromScene(point)));
    });
    connect(editGraph_, &QtNodes::AbstractGraphModel::nodeDeleted, this, [this](Id id) { if (selected_ == id) { selected_ = 0; textPane_->hide(); selectNode(0); } });
    connect(editGraph_, &Graph::textEditRequested, this, [this](Id id, const QString &field) { selectNode(id); showText(field); });
    connect(editGraph_, &Graph::nodeActivated, this, [this](Id id) { selectNode(id); });
    textPane_ = new QWidget; textPane_->setObjectName("textEditorPane"); auto *textLayout = new QVBoxLayout(textPane_);
    auto *heading = new QHBoxLayout; textTitle_ = new QLabel; heading->addWidget(textTitle_, 1);
    button("閉じる", heading, [this] { textPane_->hide(); field_.clear(); }); textLayout->addLayout(heading);
    text_ = new QPlainTextEdit; text_->setObjectName("parameterEditor"); text_->setFont(QFont("Consolas", 11)); new Highlighter(text_->document()); textLayout->addWidget(text_, 1);
    textError_ = new QLabel; textError_->setWordWrap(true); textError_->setStyleSheet("color:#ff9d9d"); textLayout->addWidget(textError_);
    connect(text_, &QPlainTextEdit::textChanged, this, [this] {
        if (loading_ || !editGraph_->nodeExists(selected_) || field_.isEmpty()) return;
        auto n = editGraph_->flow.nodes.at(selected_);
        if (field_ == "schema") n.schema = text_->toPlainText(); else n.prompt = text_->toPlainText();
        editGraph_->updateNode(n); textError_->clear();
        if (field_ == "schema" && !n.schema.trimmed().isEmpty()) {
            try { checkSchema(parseJson(n.schema)); } catch (const std::exception &e) { textError_->setText(QString::fromUtf8(e.what())); }
        }
    });
    editorSplit_->addWidget(textPane_); textPane_->hide(); editorSplit_->setSizes({190, 830, 360}); editorSplit_->setStretchFactor(1, 1);
    tabs_->addTab(editorSplit_, "フロー編集");
    auto *workSplit = new QSplitter;
    auto *projectPane = new QWidget; auto *pl = new QVBoxLayout(projectPane); pl->addWidget(new QLabel("プロジェクト"));
    projectList_ = new QListWidget; pl->addWidget(projectList_, 1);
    connect(projectList_, &QListWidget::currentRowChanged, this, [this](int rowIndex) {
        if (rowIndex >= 0 && projectList_->item(rowIndex)) selectProject(projectList_->item(rowIndex)->data(Qt::UserRole).toInt());
    });
    button("＋ ディレクトリを追加…", pl, [this] { addProject(); });
    workSplit->addWidget(projectPane);
    auto *center = new QWidget; auto *cl = new QVBoxLayout(center);
    auto *toolbar = new QHBoxLayout; status_ = new QLabel("プロジェクトを選択してください"); status_->setWordWrap(true); toolbar->addWidget(status_, 1);
    runButton_ = button("▶ 実行", toolbar, [this] { executeFlow(); });
    stopButton_ = button("■ 停止", toolbar, [this] { runner_->cancel(); }); stopButton_->setEnabled(false); cl->addLayout(toolbar);
    workScene_ = new QtNodes::BasicGraphicsScene(*workGraph_, this); workView_ = new GraphView(workScene_); workView_->setScaleRange(0.15, 3); cl->addWidget(workView_, 1);
    connect(workScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectWorkNode(id); });
    connect(workGraph_, &Graph::nodeActivated, this, [this](Id id) { selectWorkNode(id); });
    connect(workGraph_, &Graph::textEditRequested, this, [this](Id id, const QString &) { selectWorkNode(id); });
    auto tooltips = [this](Graph *graph, QtNodes::BasicGraphicsScene *scene) {
        auto update = [graph, scene](Id id) {
            if (auto *item = scene->nodeGraphicsObject(id)) {
                item->setToolTip(nodeDescription(graph->flow.nodes.at(id).kind));
                if (graph->readOnly) item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            }
        };
        connect(graph, &QtNodes::AbstractGraphModel::nodeCreated, this, update);
        connect(graph, &QtNodes::AbstractGraphModel::modelReset, this, [graph, update] { for (auto &[id, node] : graph->flow.nodes) update(id); });
    };
    tooltips(editGraph_, editScene_); tooltips(workGraph_, workScene_);
    workSplit->addWidget(center);
    auto *details = new QWidget; auto *dl = new QVBoxLayout(details);
    nodeStatus_ = new QLabel("ノードを選択してください"); nodeStatus_->setWordWrap(true); dl->addWidget(nodeStatus_);
    auto *detailSplit = new QSplitter(Qt::Vertical); detailSplit->setObjectName("workDetailsSplitter"); dl->addWidget(detailSplit, 1);
    auto *historyPane = new QWidget; auto *hl = new QVBoxLayout(historyPane); hl->setContentsMargins(0, 0, 0, 0);
    auto *historyHeading = new QHBoxLayout; historyHeading->addWidget(new QLabel("実行履歴"), 1);
    followHistoryButton_ = new QToolButton; followHistoryButton_->setObjectName("followLatestExecution");
    followHistoryButton_->setText("最新を追従"); followHistoryButton_->setCheckable(true); followHistoryButton_->setChecked(true);
    followHistoryButton_->setToolTip("最新のノード実行に自動で切り替えます。履歴を選択すると追従を停止します。");
    historyHeading->addWidget(followHistoryButton_); hl->addLayout(historyHeading);
    connect(followHistoryButton_, &QToolButton::toggled, this, [this](bool follow) { followLatestHistory_ = follow; if (follow) refreshWorkHistory(); });
    historyList_ = new QListWidget; historyList_->setObjectName("nodeExecutionHistory");
    historyList_->setToolTip("選択したノードの実行を日時順（新しい順）に表示します。ノード未選択時は表示しません。"); hl->addWidget(historyList_, 1); detailSplit->addWidget(historyPane);
    connect(historyList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        selectedHistory_ = item ? item->data(Qt::UserRole).toString() : QString();
        if (item) { followLatestHistory_ = false; followHistoryButton_->setChecked(false); }
        showWorkResult();
    });
    connect(historyList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *) { followLatestHistory_ = false; followHistoryButton_->setChecked(false); });
    auto *logPane = new QWidget; auto *ll = new QVBoxLayout(logPane); ll->setContentsMargins(0, 8, 0, 0);
    ll->addWidget(new QLabel("進行状況 / CLIログ")); log_ = new ProgressView; ll->addWidget(log_, 1); detailSplit->addWidget(logPane);
    auto *outputPane = new QWidget; auto *ol = new QVBoxLayout(outputPane); ol->setContentsMargins(0, 8, 0, 0);
    ol->addWidget(new QLabel("次段への出力")); output_ = new ContentView;
    auto *outputScroll = new QScrollArea; outputScroll->setWidgetResizable(true); outputScroll->setWidget(output_); ol->addWidget(outputScroll, 1); detailSplit->addWidget(outputPane);
    detailSplit->setSizes({180, 280, 220});
    workSplit->addWidget(details); workSplit->setSizes({230, 780, 330}); workSplit->setStretchFactor(1, 1);
    tabs_->addTab(workSplit, "プロジェクト作業"); setCentralWidget(root);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 1 || runner_->running()) return;
        if (project_ >= 0) selectProject(project_);
        else { workGraph_->replace(editGraph_->flow); selectWorkNode(workGraph_->nodeExists(workSelected_) ? workSelected_ : 0); }
    });
    setStyleSheet(R"(
        QMainWindow, QWidget { background:#191f2a; color:#dce5f3; }
        QMenuBar, QMenu, QTabBar::tab { background:#222a38; }
        QMenu { padding:6px; }
        QMenu::item { padding:8px 36px 8px 20px; }
        QMenu::item:selected { background:#3b5680; }
        QPushButton { background:#2d3b50; border:1px solid #40536c; border-radius:5px; padding:7px 9px; }
        QPushButton:hover { background:#3c506d; } QPushButton:disabled { color:#738198; }
        QLineEdit, QPlainTextEdit, QTextBrowser, QTreeWidget, QListWidget, QComboBox, QSpinBox, QTableWidget { background:#121925; border:1px solid #354259; padding:4px; selection-background-color:#3b5680; }
        QTabBar::tab { padding:12px 24px; } QTabBar::tab:selected { color:#9bc7ff; border-bottom:2px solid #83b6ff; }
        QSplitter::handle { background:#344055; width:5px; } QScrollArea { border:0; }
        QWidget#flowRail { background:#141b27; border:1px solid #354259; border-radius:14px; }
        QScrollArea#flowScroll { background:transparent; border:0; }
        QToolButton[flowTile="true"] { background:#2b3b53; color:#dce9ff; border:2px solid transparent; border-radius:14px; padding:0; font-size:17px; font-weight:bold; }
        QToolButton[flowTile="true"]:hover { background:#435c7e; border-color:#87afd9; }
        QToolButton[flowTile="true"]:checked { background:#345580; border:3px solid #b5d5ff; }
        QToolButton[flowTile="true"]:checked:hover { background:#466f9f; border-color:#e5f1ff; }
        QToolButton#addFlowTile { background:#202c3e; color:#acd4ff; border:2px dashed #536c8f; font-size:28px; }
        QToolButton#addFlowTile:hover { background:#3c5679; border-color:#c2dfff; color:white; }
    )");
    statusBar()->showMessage("フローを作成するか、既存のJSONフローを開いてください");
}
void Window::changed() {
    if (loading_ || current_ < 0) return;
    auto &doc = documents_[size_t(current_)]; doc.flow = editGraph_->flow; doc.dirty = true;
    for (auto &p : projects_) if (p.flowPath == doc.path) p.results.clear();
    setWindowTitle("AgentNodify — " + doc.flow.name + " *");
}
void Window::refreshActivities() {
    while (auto *item = activityLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
        delete item;
    }
    auto tile = [this](const QString &text, const QString &name, std::function<void()> action) {
        auto *button = new QToolButton; button->setProperty("flowTile", true); button->setFixedSize(56, 56);
        button->setText(text); button->setAccessibleName(name); button->setCursor(Qt::PointingHandCursor);
        activityLayout_->addWidget(button, 0, Qt::AlignHCenter); connect(button, &QToolButton::clicked, this, std::move(action));
        return button;
    };
    for (int i = 0; i < int(documents_.size()); ++i) {
        const auto &doc = documents_[size_t(i)];
        auto *button = tile(doc.flow.name.left(2), doc.flow.name, [this, i] { selectFlow(i); });
        button->setObjectName("flowTile"); button->setCheckable(true); button->setChecked(i == current_);
        button->setToolTip(doc.flow.name + "\n" + doc.path);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this, [this, button, i](const QPoint &point) {
            if (runner_->running()) return;
            QMenu menu(this);
            menu.addAction("名前を変更", this, [this, i] {
                selectFlow(i); bool ok = false;
                auto name = QInputDialog::getText(this, "フロー名", "名前", QLineEdit::Normal, editGraph_->flow.name, &ok);
                if (ok && !name.trimmed().isEmpty()) { editGraph_->flow.name = name.trimmed(); changed(); refreshActivities(); }
            });
            menu.addAction("一覧から削除", this, [this, i] { removeFlow(i); });
            menu.exec(button->mapToGlobal(point));
        });
    }
    auto *add = tile("＋", "フローを追加", [] {}); add->setObjectName("addFlowTile"); add->setToolTip("フローを追加");
    auto *menu = new QMenu(add);
    menu->addAction("新規フロー", this, [this] { newFlow(); });
    menu->addAction("既存フローの追加", this, [this] { openFlow(); });
    add->setMenu(menu); add->setPopupMode(QToolButton::InstantPopup);
    activityLayout_->addStretch();
}
void Window::selectFlow(int index) {
    if (index < 0 || index >= int(documents_.size()) || runner_->running()) return;
    current_ = index; loading_ = true;
    editGraph_->replace(documents_[size_t(index)].flow); selected_ = 0; field_.clear(); textPane_->hide();
    loading_ = false; selectNode(0); editView_->centerScene();
    setWindowTitle("AgentNodify — " + editGraph_->flow.name + (documents_[size_t(index)].dirty ? " *" : ""));
    refreshActivities(); refreshProjects();
}
void Window::selectNode(Id id) {
    selected_ = id; field_.clear(); textPane_->hide();
}
void Window::showText(const QString &field) {
    if (!editGraph_->nodeExists(selected_)) return;
    field_ = field; loading_ = true; textError_->clear();
    textTitle_->setText(field == "schema" ? "出力スキーマ · JSON Schema Draft 7" : "入力プロンプト");
    const auto &n = editGraph_->flow.nodes.at(selected_); text_->setPlainText(field == "schema" ? n.schema : n.prompt);
    loading_ = false; textPane_->show(); text_->setFocus();
    const auto id = selected_;
    QTimer::singleShot(0, this, [this, id] {
        if (selected_ == id && textPane_->isVisible())
            if (auto *item = editScene_->nodeGraphicsObject(id)) editView_->ensureVisible(item, 30, 30);
    });
}
void Window::newFlow() {
    if (runner_->running()) return;
    Document doc; doc.flow = Flow::initial(); doc.flow.name = QString("フロー %1").arg(documents_.size() + 1); doc.path = uniquePath(dataDir_ + "/flows"); doc.dirty = true;
    documents_.push_back(doc); selectFlow(int(documents_.size()) - 1);
}
void Window::removeFlow(int index) {
    if (runner_->running() || index < 0 || index >= int(documents_.size())) return;
    auto &doc = documents_[size_t(index)];
    const auto internalDir = QFileInfo(dataDir_ + "/flows").canonicalFilePath();
    const bool internal = QFileInfo(doc.path).absoluteDir().canonicalPath().compare(internalDir, Qt::CaseInsensitive) == 0;
    const auto warning = internal
        ? "このフローはアプリ内部にのみ保存されています。フロー定義のデータも削除します。元に戻せません。\n一覧から削除しますか？"
        : "一覧から削除しますか？外部のフロー定義ファイルは削除しません。";
    if (QMessageBox::warning(this, "フローを一覧から削除", warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (internal && QFileInfo::exists(doc.path) && !QFile::remove(doc.path)) { reportError("フロー定義を削除できません: " + doc.path); return; }
    if (!internal && doc.dirty && QMessageBox::warning(this, "未保存の変更", "未保存の変更は失われます。外部ファイルは変更せず一覧から削除しますか？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    documents_.erase(documents_.begin() + index); current_ = -1;
    if (documents_.empty()) newFlow(); else selectFlow(qMin(index, int(documents_.size()) - 1));
    persist();
}
void Window::openFlow() {
    if (runner_->running()) return;
    auto path = QFileDialog::getOpenFileName(this, "フローを開く", {}, "JSON flow (*.json)"); if (path.isEmpty()) return;
    for (int i = 0; i < int(documents_.size()); ++i) if (documents_[size_t(i)].path == path) { selectFlow(i); return; }
    try {
        auto flow = Flow::load(path);
        // Invalid definitions remain editable, but dangling edges are not safe to render.
        for (auto &e : flow.edges) if (!flow.nodes.contains(e.from) || !flow.nodes.contains(e.to) || e.input >= flow.inputCount(flow.nodes.at(e.to)) || e.output >= flow.outputCount(flow.nodes.at(e.from)))
            throw std::runtime_error("描画できない接続があります（存在しないノードまたはポート）");
        documents_.push_back({flow, path, false}); selectFlow(int(documents_.size()) - 1); persist();
    } catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); }
}
bool Window::saveFlow(bool saveAs) {
    if (current_ < 0 || runner_->running()) return false;
    auto &doc = documents_[size_t(current_)]; auto path = doc.path;
    if (saveAs) { path = QFileDialog::getSaveFileName(this, "フローを保存", path, "JSON flow (*.json)"); if (path.isEmpty()) return false; }
    try {
        editGraph_->flow.save(path);
        for (auto &p : projects_) if (p.flowPath == doc.path) p.flowPath = path;
        doc.path = path; doc.flow = editGraph_->flow; doc.dirty = false;
        refreshActivities(); setWindowTitle("AgentNodify — " + doc.flow.name); persist(); statusBar()->showMessage("保存しました: " + path, 6000); return true;
    } catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); return false; }
}
void Window::validateFlow() {
    auto errors = editGraph_->flow.validate();
    errors.append(modelSelectionErrors(editGraph_->flow));
    if (errors.isEmpty()) QMessageBox::information(this, "フロー検証", "エラーはありません");
    else reportError(errors.join('\n'));
}
void Window::refreshProjects() {
    QSignalBlocker block(projectList_); projectList_->clear(); project_ = -1;
    if (current_ < 0) return;
    auto path = documents_[size_t(current_)].path;
    for (int i = 0; i < int(projects_.size()); ++i) if (projects_[size_t(i)].flowPath == path) {
        auto *item = new QListWidgetItem(projects_[size_t(i)].name, projectList_); item->setData(Qt::UserRole, i); item->setToolTip(projects_[size_t(i)].directory);
        auto *row = new QWidget; auto *layout = new QHBoxLayout(row); layout->setContentsMargins(6, 2, 2, 2);
        layout->addWidget(new QLabel(projects_[size_t(i)].name), 1);
        auto *more = new QToolButton; more->setText("…"); more->setObjectName("projectOpenMenu");
        more->setToolTip("ディレクトリを開く"); more->setAccessibleName("プロジェクトディレクトリを開く"); layout->addWidget(more);
        auto *menu = new QMenu(more); more->setMenu(menu); more->setPopupMode(QToolButton::InstantPopup);
        connect(menu, &QMenu::aboutToShow, this, [this, menu, i] {
            menu->clear();
            for (const auto &opener : detectProjectOpeners()) {
                menu->addAction(opener.label, this, [this, i, opener] {
                    if (i >= int(projects_.size())) return;
                    QString error; if (!openProjectDirectory(opener, projects_[size_t(i)].directory, &error)) reportError(error);
                });
            }
        });
        auto *remove = new QToolButton; remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
        remove->setObjectName("removeProject"); remove->setToolTip("一覧から削除（ディレクトリは削除しません）"); remove->setAccessibleName("プロジェクトを一覧から削除"); layout->addWidget(remove);
        item->setText(""); item->setSizeHint(row->sizeHint()); projectList_->setItemWidget(item, row);
        connect(remove, &QToolButton::clicked, this, [this, i] {
            if (runner_->running()) return;
            projects_.erase(projects_.begin() + i); project_ = -1; refreshProjects(); persist();
        });
    }
    if (projectList_->count()) { projectList_->setCurrentRow(0); selectProject(projectList_->item(0)->data(Qt::UserRole).toInt()); }
    else { workGraph_->replace(editGraph_->flow); workSelected_ = 0; selectWorkNode(0); status_->setText("プロジェクトディレクトリを追加してください"); }
}
void Window::selectProject(int index) {
    if (runner_->running() || index < 0 || index >= int(projects_.size())) return;
    const bool sameProject = project_ == index;
    project_ = index; auto &p = projects_[size_t(index)]; workGraph_->replace(editGraph_->flow);
    for (auto &[id, r] : p.results) if (workGraph_->nodeExists(id)) workGraph_->states[id] = r.state;
    for (auto &[id, n] : workGraph_->flow.nodes) emit workGraph_->nodeUpdated(id);
    workView_->centerScene(); status_->setText(p.directory);
    if (sameProject && (workSelected_ == 0 || workGraph_->nodeExists(workSelected_))) {
        workGraph_->setHighlightedNode(workSelected_); refreshWorkHistory();
    } else selectWorkNode(0);
}
void Window::selectWorkNode(Id id) {
    workSelected_ = id; selectedHistory_.clear(); followLatestHistory_ = true;
    workGraph_->setHighlightedNode(id);
    refreshWorkHistory();
}
void Window::refreshWorkHistory() {
    std::vector<const NodeExecution *> entries;
    if (project_ >= 0 && workGraph_->nodeExists(workSelected_)) for (auto it = projects_[size_t(project_)].history.rbegin(); it != projects_[size_t(project_)].history.rend(); ++it)
        if (it->node == workSelected_) entries.push_back(&*it);
    const int scroll = historyList_->verticalScrollBar()->value();
    QSignalBlocker block(historyList_);
    bool rebuild = historyList_->count() != int(entries.size());
    if (!rebuild) for (int row = 0; row < int(entries.size()); ++row)
        if (historyList_->item(row)->data(Qt::UserRole).toString() != entries[size_t(row)]->id) { rebuild = true; break; }
    if (rebuild) historyList_->clear();
    int selectedRow = -1;
    for (int row = 0; row < int(entries.size()); ++row) {
        const auto &entry = *entries[size_t(row)];
        auto *item = rebuild ? new QListWidgetItem(historyList_) : historyList_->item(row);
        item->setData(Qt::UserRole, entry.id);
        item->setText(QString("%1 · %2\n実行 %3 · %4").arg(entry.startedAt.toLocalTime().toString("yyyy/MM/dd HH:mm:ss.zzz"), stateName(entry.state)).arg(entry.executionNumber).arg(entry.agent.isEmpty() ? QString("内部処理") : entry.agent));
        item->setToolTip("開始: " + entry.startedAt.toLocalTime().toString(Qt::ISODateWithMs) + "\n終了: " +
            (entry.finishedAt.isValid() ? entry.finishedAt.toLocalTime().toString(Qt::ISODateWithMs) : entry.state == State::Running ? "実行中" : "不明（中断）"));
        if (entry.id == selectedHistory_) selectedRow = row;
    }
    if ((followLatestHistory_ || selectedRow < 0) && !entries.empty()) selectedRow = 0;
    historyList_->setCurrentRow(selectedRow);
    selectedHistory_ = selectedRow >= 0 ? entries[size_t(selectedRow)]->id : QString();
    if (followLatestHistory_) historyList_->scrollToTop(); else historyList_->verticalScrollBar()->setValue(scroll);
    QSignalBlocker followBlock(followHistoryButton_); followHistoryButton_->setChecked(followLatestHistory_); followHistoryButton_->setEnabled(!entries.empty());
    showWorkResult();
}
void Window::showWorkResult() {
    const NodeExecution *execution = nullptr;
    if (project_ >= 0 && !selectedHistory_.isEmpty()) {
        const auto &history = projects_[size_t(project_)].history;
        auto it = std::find_if(history.begin(), history.end(), [this](const auto &entry) { return entry.id == selectedHistory_; });
        if (it != history.end()) execution = &*it;
    }
    const QString key = execution ? execution->id : QString("current:%1:%2").arg(project_).arg(workSelected_);
    displayedExecution_ = key;
    auto render = [this, &key](const QString &log, State state, const Json *output = nullptr, bool structured = false, const QString &directory = QString()) {
        log_->setProgress(log, key, state, output, structured, directory);
        if (output) output_->setValue(*output, true, structured || log_->finalIsJson()); else output_->setPlainText({});
    };
    if (execution) {
        nodeStatus_->setText(execution->title + " — " + stateName(execution->state) + "\n" + execution->startedAt.toLocalTime().toString("yyyy/MM/dd HH:mm:ss.zzz"));
        render(execution->log, execution->state, execution->state == State::Succeeded ? &execution->output : nullptr, execution->structuredOutput, execution->directory); return;
    }
    if (!workGraph_->nodeExists(workSelected_)) { render({}, State::Pending); nodeStatus_->setText("ノードを選択してください"); return; }
    const auto &node = workGraph_->flow.nodes.at(workSelected_);
    const auto title = node.title.isEmpty() ? kindName(node.kind) : node.title;
    if (project_ < 0 || !projects_[size_t(project_)].results.contains(workSelected_)) { render({}, State::Pending); nodeStatus_->setText(title + " — 待機"); return; }
    const auto &r = projects_[size_t(project_)].results.at(workSelected_);
    nodeStatus_->setText(title + " — " + stateName(r.state) + QString("（実行 %1 回）").arg(r.executions));
    render(r.log, r.state, r.state == State::Succeeded ? &r.output : nullptr, !node.schema.trimmed().isEmpty());
}
void Window::recordNodeExecution(const NodeExecution &value) {
    if (project_ < 0) return;
    auto &project = projects_[size_t(project_)]; auto entry = value;
    auto it = std::find_if(project.history.begin(), project.history.end(), [&](const auto &old) { return old.id == entry.id; });
    const bool added = it == project.history.end();
    entry.sequence = added ? (project.history.empty() ? 1 : project.history.back().sequence + 1) : it->sequence;
    if (added) project.history.push_back(entry); else *it = entry;
    if (!smoke_) {
        pendingHistory_[entry.id] = {dataDir_ + "/history/" + project.historyId, entry};
        if (added || entry.state != State::Running) flushHistory();
        else if (!historySaveTimer_->isActive()) historySaveTimer_->start(1000);
    }
    if (workSelected_ == 0 || workSelected_ == entry.node) refreshWorkHistory();
}
void Window::flushHistory() {
    historySaveTimer_->stop(); auto pending = std::move(pendingHistory_); pendingHistory_.clear();
    QString error;
    for (const auto &[id, record] : pending) {
        try { saveNodeExecution(record.first, record.second); }
        catch (const std::exception &e) { pendingHistory_[id] = record; error = QString::fromUtf8(e.what()); }
    }
    if (!pendingHistory_.empty()) {
        historySaveTimer_->start(5000);
        if (!historySaveFailed_) { historySaveFailed_ = true; reportError(error); }
    } else historySaveFailed_ = false;
}
void Window::addProject() {
    if (runner_->running() || current_ < 0) return;
    auto directory = QFileDialog::getExistingDirectory(this, "プロジェクトディレクトリ"); if (directory.isEmpty()) return;
    projects_.push_back({QFileInfo(directory).fileName(), directory, documents_[size_t(current_)].path, {}});
    refreshProjects(); projectList_->setCurrentRow(projectList_->count() - 1); persist();
}
void Window::executeFlow() {
    if (project_ < 0 || runner_->running()) { status_->setText("プロジェクトを選択してください"); return; }
    auto errors = editGraph_->flow.validate(); if (!errors.isEmpty()) { reportError(errors.join('\n')); return; }
    errors = modelSelectionErrors(editGraph_->flow); if (!errors.isEmpty()) { reportError(errors.join('\n')); return; }
    if (!smoke_ && QMessageBox::question(this, "フロー実行", "エージェントはプロジェクト内のファイルを読み取り・変更する可能性があります。\n各CLIの権限設定が適用されます。実行しますか？") != QMessageBox::Yes) return;
    workGraph_->replace(editGraph_->flow); projects_[size_t(project_)].results.clear();
    persist(); // Preserve the project's history identity before the first record is written.
    activities_->setEnabled(false); projectList_->setEnabled(false); tabs_->setTabEnabled(0, false); runButton_->setEnabled(false); stopButton_->setEnabled(true);
    runner_->start(editGraph_->flow, projects_[size_t(project_)].directory, config_);
}
void Window::settings() {
    if (runner_->running()) return;
    QDialog dialog(this); dialog.setWindowTitle("CLI設定"); dialog.resize(650, 300);
    auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout;
    auto add = [this, form](const QString &label, const QString &value) {
        auto *row = new QWidget; auto *layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(value); layout->addWidget(edit, 1);
        button("参照…", layout, [this, edit] { auto p = QFileDialog::getOpenFileName(this, "CLI実行ファイル", {}, "Executable (*.exe);;All files (*)"); if (!p.isEmpty()) edit->setText(p); });
        form->addRow(label, row); return edit;
    };
    auto *codex = add("Codex", config_.codex), *claude = add("Claude Code", config_.claude), *opencode = add("OpenCode", config_.opencode);
    auto *timeout = new QSpinBox; timeout->setRange(1, 86400); timeout->setValue(config_.timeoutSeconds); form->addRow("タイムアウト（秒）", timeout); layout->addLayout(form);
    auto *write = new QCheckBox("プロジェクトへの書き込みを許可（Codex / Claude Code）"); write->setChecked(config_.workspaceWrite); layout->addWidget(write);
    auto *writeHint = new QLabel("明示的に選んだ場合のみ、Codexはworkspace-write、Claude CodeはacceptEditsを使います。\nClaude CodeのacceptEditsはOSサンドボックスではありません。CLIのdenyルールは維持されます。\nOpenCodeは既存の権限設定に従い、自動承認は追加しません。"); writeHint->setWordWrap(true); layout->addWidget(writeHint);
    auto *hint = new QLabel("認証は各CLIで事前に行ってください。権限の自動承認は付与しません。\nWindowsでは.cmdではなくネイティブ.exeを指定します。\nOpenCodeのJSON出力はスキーマ指示＋受信後検証です。"); hint->setWordWrap(true); layout->addWidget(hint);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        config_ = {codex->text(), claude->text(), opencode->text(), timeout->value()};
        config_.workspaceWrite = write->isChecked();
        persist(); if (!smoke_) { modelCatalog_->setConfig(config_); modelCatalog_->refreshAll(true); }
    }
}
void Window::reportError(const QString &message) { if (!smoke_) QMessageBox::warning(this, "エラー", message); statusBar()->showMessage(message, 15000); }
void Window::persist() {
    if (smoke_) return;
    Json paths = Json::array(), projects = Json::array();
    for (auto &d : documents_) paths.push_back(str(d.path));
    for (auto &p : projects_) projects.push_back({{"name", str(p.name)}, {"directory", str(p.directory)}, {"flow", str(p.flowPath)}, {"historyId", str(p.historyId)}});
    Json j = {{"flows", paths}, {"projects", projects}, {"codex", str(config_.codex)}, {"claude", str(config_.claude)}, {"opencode", str(config_.opencode)}, {"timeout", config_.timeoutSeconds}};
    j["workspaceWrite"] = config_.workspaceWrite;
    QSaveFile file(dataDir_ + "/workspace.json"); auto bytes = QByteArray::fromStdString(j.dump(2));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) reportError("ワークスペース設定を保存できません: " + file.errorString());
}
void Window::restore() {
    QFile file(dataDir_ + "/workspace.json"); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) { reportError(file.errorString()); return; }
    try {
        auto j = Json::parse(file.readAll().toStdString());
        config_ = {qs(j.at("codex")), qs(j.at("claude")), qs(j.at("opencode")), qBound(1, j.value("timeout", 600), 86400)};
        config_.workspaceWrite = j.value("workspaceWrite", false);
#ifdef Q_OS_WIN
        // Migrate only our old bare default, never a user-specified executable path.
        if (config_.opencode == "opencode") config_.opencode = "opencode-cli";
#endif
        for (auto &path : j.at("flows")) {
            try {
                auto flow = Flow::load(qs(path));
                for (auto &e : flow.edges) if (!flow.nodes.contains(e.from) || !flow.nodes.contains(e.to) || e.input >= flow.inputCount(flow.nodes.at(e.to)) || e.output >= flow.outputCount(flow.nodes.at(e.from))) throw std::runtime_error("不正な接続");
                documents_.push_back({flow, qs(path), false});
            } catch (const std::exception &e) { reportError(qs(path) + ": " + QString::fromUtf8(e.what())); }
        }
        for (auto &p : j.at("projects")) {
            Project project{qs(p.at("name")), qs(p.at("directory")), qs(p.at("flow")), {}};
            if (p.contains("historyId") && p.at("historyId").is_string()) {
                const auto uuid = QUuid(qs(p.at("historyId")));
                if (!uuid.isNull()) project.historyId = uuid.toString(QUuid::WithoutBraces);
            }
            QStringList errors; project.history = loadNodeExecutions(dataDir_ + "/history/" + project.historyId, &errors);
            for (const auto &entry : project.history) {
                auto &result = project.results[entry.node]; ++result.executions;
                result.state = entry.state; result.log = entry.log; result.output = entry.output;
            }
            projects_.push_back(std::move(project));
            if (!errors.isEmpty()) reportError("一部の実行履歴を読み込めませんでした:\n" + errors.join('\n'));
        }
    } catch (const std::exception &e) { reportError("設定の読み込みに失敗: " + QString::fromUtf8(e.what())); }
}
void Window::closeEvent(QCloseEvent *event) {
    if (runner_->running()) {
        if (QMessageBox::question(this, "終了", "実行を中止して終了しますか？") != QMessageBox::Yes) { event->ignore(); return; }
        runner_->cancel();
    }
    flushHistory();
    if (!pendingHistory_.empty() && QMessageBox::warning(this, "実行履歴の保存", "実行履歴を保存できていません。未保存の履歴を失っても終了しますか？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    // Preserve every modified flow, not only the currently selected activity.
    for (auto &doc : documents_) if (doc.dirty && !smoke_) {
        try { doc.flow.save(doc.path); doc.dirty = false; }
        catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); event->ignore(); return; }
    }
    persist(); event->accept();
}
bool Window::smokeTest() {
    auto id = editGraph_->addNode("agent"); if (id == QtNodes::InvalidNodeId) return false;
    selectNode(id); showText("prompt"); text_->setPlainText("Say hello");
    editGraph_->addConnection({1, 0, id, 0}); editGraph_->addConnection({id, 0, 2, 0});
    editGraph_->addConnection({id, 0, id, 0});
    if (!editGraph_->connectionExists({id, 0, id, 0})) return false;
    editGraph_->deleteConnection({id, 0, id, 0});
    if (!editGraph_->flow.validate().isEmpty()) return false;
    workGraph_->replace(editGraph_->flow); tabs_->setCurrentIndex(1); selectWorkNode(id);
    const auto count = workGraph_->flow.nodes.size();
    if (workGraph_->addNode("agent") != QtNodes::InvalidNodeId || workGraph_->flow.nodes.size() != count) return false;
    if (workGraph_->deleteNode(id) || workGraph_->setNodeData(id, QtNodes::NodeRole::Position, QPointF(1, 1))) return false;
    return true;
}
}
