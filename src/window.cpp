#include "window.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QUuid>
#include <QVBoxLayout>
#include <QRegularExpression>

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
}
Window::Window(bool smoke) : smoke_(smoke) {
    setWindowTitle("AgentNodify"); resize(1440, 900);
    dataDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir_ + "/flows");
    editGraph_ = new Graph; editGraph_->setParent(this);
    workGraph_ = new Graph; workGraph_->setParent(this); workGraph_->readOnly = true;
    runner_ = new Runner(this);
    buildUi();
    connect(editGraph_, &Graph::edited, this, [this] { changed(); });
    connect(runner_, &Runner::changed, this, [this](Id id) {
        if (project_ < 0 || !runner_->results().contains(id)) return;
        projects_[size_t(project_)].results = runner_->results();
        workGraph_->states[id] = runner_->results().at(id).state; emit workGraph_->nodeUpdated(id);
        if (workSelected_ == id) selectWorkNode(id);
    });
    connect(runner_, &Runner::finished, this, [this](bool ok, const QString &message) {
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
    if (documents_.empty()) newFlow(); else { refreshActivities(); selectFlow(0); }
}
void Window::buildUi() {
    auto *file = menuBar()->addMenu("ファイル");
    file->addAction("新しいフロー", QKeySequence::New, this, [this] { newFlow(); });
    file->addAction("開く…", QKeySequence::Open, this, [this] { openFlow(); });
    file->addAction("保存", QKeySequence::Save, this, [this] { saveFlow(); });
    file->addAction("名前を付けて保存…", QKeySequence::SaveAs, this, [this] { saveFlow(true); });
    file->addAction("フロー名を変更…", this, [this] {
        if (runner_->running()) return;
        bool ok; auto name = QInputDialog::getText(this, "フロー名", "名前", QLineEdit::Normal, editGraph_->flow.name, &ok);
        if (ok && !name.trimmed().isEmpty()) { editGraph_->flow.name = name; changed(); refreshActivities(); }
    });
    file->addSeparator(); file->addAction("終了", QKeySequence::Quit, this, &QWidget::close);
    auto *tools = menuBar()->addMenu("ツール");
    tools->addAction("フローを検証", QKeySequence("F7"), this, [this] { validateFlow(); });
    tools->addAction("接続を追加…（自己ループ可）", this, [this] { connectNodes(); });
    tools->addAction("CLI設定…", this, [this] { settings(); });
    auto *help = menuBar()->addMenu("ヘルプ");
    help->addAction("操作方法", this, [this] {
        QMessageBox::information(this, "操作方法", "左のボタンでノードを追加し、丸いポートをドラッグして接続します。\nノードをクリックしてパラメータを編集します。Deleteで選択要素を削除できます。\nAND入力は0-indexed、分岐はifの番号順・最後がelseです。\nポート数を変えると、そのノードの接続は解除されます。\nフローを保存し、プロジェクト作業タブでディレクトリを追加して実行します。\nCLIの認証と権限設定は各CLIで事前に行ってください。");
    });
    auto *root = new QWidget; auto *row = new QHBoxLayout(root); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0);
    auto *bar = new QWidget; bar->setFixedWidth(112); auto *barLayout = new QVBoxLayout(bar);
    auto *brand = new QLabel("AN"); brand->setAlignment(Qt::AlignCenter); brand->setStyleSheet("font-size:24px;color:#87b9ff;font-weight:bold;padding:12px;"); barLayout->addWidget(brand);
    activities_ = new QListWidget; activities_->setWordWrap(true); barLayout->addWidget(activities_, 1);
    connect(activities_, &QListWidget::currentRowChanged, this, [this](int index) { selectFlow(index); });
    button("＋", barLayout, [this] { newFlow(); }); row->addWidget(bar);
    tabs_ = new QTabWidget; row->addWidget(tabs_, 1);
    editorSplit_ = new QSplitter;
    auto *left = new QWidget; auto *leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new QLabel("ノードを追加"));
    for (Kind k : {Kind::Start, Kind::Agent, Kind::And, Kind::Branch, Kind::End})
        button("＋ " + kindName(k), leftLayout, [this, k] {
            auto id = editGraph_->addNode(kindKey(k));
            if (id == QtNodes::InvalidNodeId) statusBar()->showMessage("始端・終端はそれぞれ1つだけです", 5000);
            else selectNode(id);
        });
    button("接続を追加…", leftLayout, [this] { connectNodes(); });
    button("フローを検証", leftLayout, [this] { validateFlow(); });
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
    parameters_ = new QWidget; parameters_->setLayout(new QVBoxLayout); scroll->setWidget(parameters_); leftLayout->addWidget(scroll, 1);
    left->setMinimumWidth(230); editorSplit_->addWidget(left);
    editScene_ = new QtNodes::BasicGraphicsScene(*editGraph_, this); editView_ = new QtNodes::GraphicsView(editScene_);
    editView_->setScaleRange(0.15, 3); editorSplit_->addWidget(editView_);
    connect(editScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectNode(id); });
    connect(editGraph_, &QtNodes::AbstractGraphModel::nodeDeleted, this, [this](Id id) { if (selected_ == id) { selected_ = 0; textPane_->hide(); selectNode(0); } });
    textPane_ = new QWidget; auto *textLayout = new QVBoxLayout(textPane_);
    auto *heading = new QHBoxLayout; textTitle_ = new QLabel; heading->addWidget(textTitle_, 1);
    button("閉じる", heading, [this] { textPane_->hide(); field_.clear(); }); textLayout->addLayout(heading);
    text_ = new QPlainTextEdit; text_->setFont(QFont("Consolas", 11)); new Highlighter(text_->document()); textLayout->addWidget(text_, 1);
    textError_ = new QLabel; textError_->setWordWrap(true); textError_->setStyleSheet("color:#ff9d9d"); textLayout->addWidget(textError_);
    connect(text_, &QPlainTextEdit::textChanged, this, [this] {
        if (loading_ || !editGraph_->nodeExists(selected_) || field_.isEmpty()) return;
        auto &n = editGraph_->flow.nodes.at(selected_);
        if (field_ == "schema") n.schema = text_->toPlainText(); else n.prompt = text_->toPlainText();
        changed(); textError_->clear();
        if (field_ == "schema" && !n.schema.trimmed().isEmpty()) {
            try { checkSchema(parseJson(n.schema)); } catch (const std::exception &e) { textError_->setText(QString::fromUtf8(e.what())); }
        }
    });
    editorSplit_->addWidget(textPane_); textPane_->hide(); editorSplit_->setSizes({260, 830, 360}); editorSplit_->setStretchFactor(1, 1);
    tabs_->addTab(editorSplit_, "フロー編集");
    auto *workSplit = new QSplitter;
    auto *projectPane = new QWidget; auto *pl = new QVBoxLayout(projectPane); pl->addWidget(new QLabel("プロジェクト"));
    projectList_ = new QListWidget; pl->addWidget(projectList_, 1);
    connect(projectList_, &QListWidget::currentRowChanged, this, [this](int rowIndex) {
        if (rowIndex >= 0 && projectList_->item(rowIndex)) selectProject(projectList_->item(rowIndex)->data(Qt::UserRole).toInt());
    });
    button("＋ ディレクトリを追加…", pl, [this] { addProject(); });
    button("一覧から削除", pl, [this] {
        if (project_ < 0 || runner_->running()) return;
        projects_.erase(projects_.begin() + project_); project_ = -1; refreshProjects(); persist();
    });
    workSplit->addWidget(projectPane);
    auto *center = new QWidget; auto *cl = new QVBoxLayout(center);
    auto *toolbar = new QHBoxLayout; status_ = new QLabel("プロジェクトを選択してください"); status_->setWordWrap(true); toolbar->addWidget(status_, 1);
    runButton_ = button("▶ 実行", toolbar, [this] { executeFlow(); });
    stopButton_ = button("■ 停止", toolbar, [this] { runner_->cancel(); }); stopButton_->setEnabled(false); cl->addLayout(toolbar);
    workScene_ = new QtNodes::BasicGraphicsScene(*workGraph_, this); workView_ = new QtNodes::GraphicsView(workScene_); workView_->setScaleRange(0.15, 3); cl->addWidget(workView_, 1);
    connect(workScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectWorkNode(id); });
    workSplit->addWidget(center);
    auto *details = new QWidget; auto *dl = new QVBoxLayout(details);
    nodeStatus_ = new QLabel("ノードを選択してください"); nodeStatus_->setWordWrap(true); dl->addWidget(nodeStatus_);
    dl->addWidget(new QLabel("進行状況 / CLIログ")); log_ = new QPlainTextEdit; log_->setReadOnly(true); dl->addWidget(log_, 1);
    dl->addWidget(new QLabel("次段への出力")); output_ = new QPlainTextEdit; output_->setReadOnly(true); new Highlighter(output_->document()); dl->addWidget(output_, 1);
    workSplit->addWidget(details); workSplit->setSizes({230, 780, 330}); workSplit->setStretchFactor(1, 1);
    tabs_->addTab(workSplit, "プロジェクト作業"); setCentralWidget(root);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != 1 || runner_->running()) return;
        if (project_ >= 0) selectProject(project_);
        else workGraph_->replace(editGraph_->flow);
    });
    setStyleSheet(R"(
        QMainWindow, QWidget { background:#191f2a; color:#dce5f3; }
        QMenuBar, QMenu, QTabBar::tab { background:#222a38; }
        QPushButton { background:#2d3b50; border:1px solid #40536c; border-radius:5px; padding:7px 9px; }
        QPushButton:hover { background:#3c506d; } QPushButton:disabled { color:#738198; }
        QLineEdit, QPlainTextEdit, QListWidget, QComboBox, QSpinBox, QTableWidget { background:#121925; border:1px solid #354259; padding:4px; selection-background-color:#3b5680; }
        QTabBar::tab { padding:12px 24px; } QTabBar::tab:selected { color:#9bc7ff; border-bottom:2px solid #83b6ff; }
        QSplitter::handle { background:#344055; width:5px; } QScrollArea { border:0; }
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
    QSignalBlocker block(activities_); activities_->clear();
    for (auto &doc : documents_) { auto *item = new QListWidgetItem(doc.flow.name, activities_); item->setToolTip(doc.path); item->setSizeHint(QSize(80, 64)); }
    activities_->setCurrentRow(current_);
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
    auto *layout = parameters_->layout();
    while (auto *item = layout->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    if (!editGraph_->nodeExists(id)) { layout->addWidget(new QLabel("ノードを選択して編集")); return; }
    const auto n = editGraph_->flow.nodes.at(id);
    layout->addWidget(new QLabel(kindName(n.kind) + QString(" #%1").arg(id)));
    auto *title = new QLineEdit(n.title); layout->addWidget(new QLabel("名前")); layout->addWidget(title);
    connect(title, &QLineEdit::editingFinished, this, [this, title, id] {
        if (!editGraph_->nodeExists(id)) return;
        editGraph_->flow.nodes.at(id).title = title->text(); emit editGraph_->nodeUpdated(id); changed();
    });
    if (n.kind == Kind::Agent) {
        auto *agent = new QComboBox; agent->addItems({"Codex", "Claude Code", "OpenCode"}); agent->setCurrentText(n.agent);
        layout->addWidget(new QLabel("エージェント")); layout->addWidget(agent);
        auto *model = new QComboBox; model->setEditable(true);
        auto populate = [model](const QString &a, const QString &value) {
            QSignalBlocker block(model); model->clear(); model->addItem("");
            if (a == "Codex") model->addItems({"gpt-6-luna"});
            else if (a == "Claude Code") model->addItems({"haiku", "claude-haiku-5-5"});
            else model->addItems({"openai/gpt-6-luna", "anthropic/claude-haiku-5-5"});
            model->setCurrentText(value); model->setToolTip("空欄はCLI既定。モデルIDは利用環境に合わせて入力できます。");
        }; populate(n.agent, n.model);
        layout->addWidget(new QLabel("モデル（入力可）")); layout->addWidget(model);
        connect(agent, &QComboBox::currentTextChanged, this, [this, id, populate](const QString &value) {
            if (!editGraph_->nodeExists(id)) return;
            auto &node = editGraph_->flow.nodes.at(id); node.agent = value; node.model.clear(); populate(value, ""); changed();
        });
        connect(model, &QComboBox::currentTextChanged, this, [this, id](const QString &value) {
            if (editGraph_->nodeExists(id)) { editGraph_->flow.nodes.at(id).model = value; changed(); }
        });
        button("入力プロンプトを編集…", layout, [this] { showText("prompt"); });
        button("出力JSON Schemaを編集…", layout, [this] { showText("schema"); });
        auto *hint = new QLabel("変数: ${name} / ${obj.key}\n配列: ${items[0]} / ${[0].name}\nスキーマ省略時はテキスト出力"); hint->setWordWrap(true); layout->addWidget(hint);
    } else if (n.kind == Kind::And) {
        auto *count = new QSpinBox; count->setRange(1, 256); count->setValue(int(n.inputs));
        layout->addWidget(new QLabel("入力ポート数")); layout->addWidget(count);
        connect(count, &QSpinBox::valueChanged, this, [this, id](int value) { auto node = editGraph_->flow.nodes.at(id); node.inputs = unsigned(value); editGraph_->updateNode(node); });
        auto *hint = new QLabel("全入力の発火を待ち、入力番号順の配列を出力します。\n発火後はリセットし、新しい全入力を待ちます。\n先着した入力はFIFOで保持します。\nポート数変更時は再接続してください。"); hint->setWordWrap(true); layout->addWidget(hint);
    } else if (n.kind == Kind::Branch) {
        button("条件を編集…", layout, [this] { editRules(); });
        auto *hint = new QLabel("if 0 → if 1 → … → else\n最初に一致した出力に伝播します。\n比較値は文字列として比較します。"); hint->setWordWrap(true); layout->addWidget(hint);
    }
    if (n.kind != Kind::Start && n.kind != Kind::End) button("ノードを削除", layout, [this, id] { editGraph_->deleteNode(id); });
    static_cast<QVBoxLayout *>(layout)->addStretch();
}
void Window::showText(const QString &field) {
    if (!editGraph_->nodeExists(selected_)) return;
    field_ = field; loading_ = true; textError_->clear();
    textTitle_->setText(field == "schema" ? "出力スキーマ · JSON Schema Draft 7" : "入力プロンプト");
    const auto &n = editGraph_->flow.nodes.at(selected_); text_->setPlainText(field == "schema" ? n.schema : n.prompt);
    loading_ = false; textPane_->show(); text_->setFocus();
}
void Window::editRules() {
    if (!editGraph_->nodeExists(selected_)) return;
    auto node = editGraph_->flow.nodes.at(selected_);
    QDialog dialog(this); dialog.setWindowTitle("分岐条件 — 上から順に評価、末尾はelse"); dialog.resize(700, 400);
    auto *layout = new QVBoxLayout(&dialog);
    auto *table = new QTableWidget(0, 3); table->setHorizontalHeaderLabels({"条件変数", "比較", "条件値（文字列）"}); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); layout->addWidget(table);
    auto add = [table](const Rule &r) {
        int row = table->rowCount(); table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(r.variable)); table->setItem(row, 2, new QTableWidgetItem(r.value));
        auto *combo = new QComboBox; combo->addItems({"等しい", "異なる"}); combo->setCurrentIndex(r.equals ? 0 : 1); table->setCellWidget(row, 1, combo);
    };
    for (auto &r : node.rules) add(r);
    auto *buttons = new QHBoxLayout; button("＋ 条件", buttons, [add] { add({"", "", true}); });
    button("− 選択行", buttons, [table] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); }); layout->addLayout(buttons);
    layout->addWidget(new QLabel("条件数を変更すると、このノードの接続は解除されます。最後の出力は常にelseです。"));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    node.rules.clear();
    for (int i = 0; i < table->rowCount(); ++i) node.rules.push_back({table->item(i, 0)->text(), table->item(i, 2)->text(), static_cast<QComboBox *>(table->cellWidget(i, 1))->currentIndex() == 0});
    editGraph_->updateNode(node);
}
void Window::connectNodes() {
    if (runner_->running()) return;
    QDialog dialog(this); dialog.setWindowTitle("接続を追加 — 自己ループも指定可能");
    auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout;
    auto *from = new QComboBox, *to = new QComboBox;
    auto *output = new QSpinBox, *input = new QSpinBox;
    for (auto &[id, node] : editGraph_->flow.nodes) {
        auto label = QString("%1 · %2").arg(id).arg(node.title);
        if (editGraph_->flow.outputCount(node)) from->addItem(label, id);
        if (editGraph_->flow.inputCount(node)) to->addItem(label, id);
    }
    auto refresh = [this, from, to, output, input] {
        if (from->currentIndex() >= 0) output->setRange(0, int(editGraph_->flow.outputCount(editGraph_->flow.nodes.at(from->currentData().toUInt()))) - 1);
        if (to->currentIndex() >= 0) input->setRange(0, int(editGraph_->flow.inputCount(editGraph_->flow.nodes.at(to->currentData().toUInt()))) - 1);
    };
    if (selected_ != 0) {
        from->setCurrentIndex(qMax(0, from->findData(selected_))); to->setCurrentIndex(qMax(0, to->findData(selected_)));
    }
    connect(from, &QComboBox::currentIndexChanged, &dialog, refresh);
    connect(to, &QComboBox::currentIndexChanged, &dialog, refresh); refresh();
    form->addRow("出力元ノード", from); form->addRow("出力ポート（0-indexed）", output);
    form->addRow("入力先ノード", to); form->addRow("入力ポート（0-indexed）", input); layout->addLayout(form);
    layout->addWidget(new QLabel("通常ノードは入力ごとに実行し、ANDは各入力から1件ずつ待ち合わせます。"));
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(box);
    box->button(QDialogButtonBox::Ok)->setEnabled(from->count() && to->count());
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    QtNodes::ConnectionId edge{from->currentData().toUInt(), unsigned(output->value()), to->currentData().toUInt(), unsigned(input->value())};
    if (!editGraph_->connectionPossible(edge)) { reportError("接続できません（重複した接続または不正なポート）"); return; }
    editGraph_->addConnection(edge);
}
void Window::newFlow() {
    if (runner_->running()) return;
    Document doc; doc.flow = Flow::initial(); doc.flow.name = QString("フロー %1").arg(documents_.size() + 1); doc.path = uniquePath(dataDir_ + "/flows"); doc.dirty = true;
    documents_.push_back(doc); refreshActivities(); selectFlow(int(documents_.size()) - 1);
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
    if (errors.isEmpty()) QMessageBox::information(this, "フロー検証", "エラーはありません");
    else reportError(errors.join('\n'));
}
void Window::refreshProjects() {
    QSignalBlocker block(projectList_); projectList_->clear(); project_ = -1;
    if (current_ < 0) return;
    auto path = documents_[size_t(current_)].path;
    for (int i = 0; i < int(projects_.size()); ++i) if (projects_[size_t(i)].flowPath == path) {
        auto *item = new QListWidgetItem(projects_[size_t(i)].name, projectList_); item->setData(Qt::UserRole, i); item->setToolTip(projects_[size_t(i)].directory);
    }
    if (projectList_->count()) { projectList_->setCurrentRow(0); selectProject(projectList_->item(0)->data(Qt::UserRole).toInt()); }
    else { workGraph_->replace(editGraph_->flow); workSelected_ = 0; selectWorkNode(0); status_->setText("プロジェクトディレクトリを追加してください"); }
}
void Window::selectProject(int index) {
    if (runner_->running() || index < 0 || index >= int(projects_.size())) return;
    project_ = index; auto &p = projects_[size_t(index)]; workGraph_->replace(editGraph_->flow);
    for (auto &[id, r] : p.results) if (workGraph_->nodeExists(id)) workGraph_->states[id] = r.state;
    for (auto &[id, n] : workGraph_->flow.nodes) emit workGraph_->nodeUpdated(id);
    workView_->centerScene(); status_->setText(p.directory); workSelected_ = 0; selectWorkNode(0);
}
void Window::selectWorkNode(Id id) {
    workSelected_ = id; log_->clear(); output_->clear();
    if (!workGraph_->nodeExists(id)) { nodeStatus_->setText("ノードを選択してください"); return; }
    QString title = workGraph_->flow.nodes.at(id).title;
    if (project_ < 0 || !projects_[size_t(project_)].results.contains(id)) { nodeStatus_->setText(title + " — 待機"); return; }
    auto &r = projects_[size_t(project_)].results.at(id);
    nodeStatus_->setText(title + " — " + stateName(r.state) + QString("（実行 %1 回）").arg(r.executions)); log_->setPlainText(r.log);
    if (r.state == State::Succeeded) output_->setPlainText(display(r.output));
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
    if (!smoke_ && QMessageBox::question(this, "フロー実行", "エージェントはプロジェクト内のファイルを読み取り・変更する可能性があります。\n各CLIの権限設定が適用されます。実行しますか？") != QMessageBox::Yes) return;
    workGraph_->replace(editGraph_->flow); projects_[size_t(project_)].results.clear();
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
    auto *hint = new QLabel("認証は各CLIで事前に行ってください。権限の自動承認は付与しません。\nWindowsでは.cmdではなくネイティブ.exeを指定します。\nOpenCodeのJSON出力はスキーマ指示＋受信後検証です。"); hint->setWordWrap(true); layout->addWidget(hint);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) { config_ = {codex->text(), claude->text(), opencode->text(), timeout->value()}; persist(); }
}
void Window::reportError(const QString &message) { if (!smoke_) QMessageBox::warning(this, "エラー", message); statusBar()->showMessage(message, 15000); }
void Window::persist() {
    if (smoke_) return;
    Json paths = Json::array(), projects = Json::array();
    for (auto &d : documents_) paths.push_back(str(d.path));
    for (auto &p : projects_) projects.push_back({{"name", str(p.name)}, {"directory", str(p.directory)}, {"flow", str(p.flowPath)}});
    Json j = {{"flows", paths}, {"projects", projects}, {"codex", str(config_.codex)}, {"claude", str(config_.claude)}, {"opencode", str(config_.opencode)}, {"timeout", config_.timeoutSeconds}};
    QSaveFile file(dataDir_ + "/workspace.json"); auto bytes = QByteArray::fromStdString(j.dump(2));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) reportError("ワークスペース設定を保存できません: " + file.errorString());
}
void Window::restore() {
    QFile file(dataDir_ + "/workspace.json"); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) { reportError(file.errorString()); return; }
    try {
        auto j = Json::parse(file.readAll().toStdString());
        config_ = {qs(j.at("codex")), qs(j.at("claude")), qs(j.at("opencode")), qBound(1, j.value("timeout", 600), 86400)};
        for (auto &path : j.at("flows")) {
            try {
                auto flow = Flow::load(qs(path));
                for (auto &e : flow.edges) if (!flow.nodes.contains(e.from) || !flow.nodes.contains(e.to) || e.input >= flow.inputCount(flow.nodes.at(e.to)) || e.output >= flow.outputCount(flow.nodes.at(e.from))) throw std::runtime_error("不正な接続");
                documents_.push_back({flow, qs(path), false});
            } catch (const std::exception &e) { reportError(qs(path) + ": " + QString::fromUtf8(e.what())); }
        }
        for (auto &p : j.at("projects")) projects_.push_back({qs(p.at("name")), qs(p.at("directory")), qs(p.at("flow")), {}});
    } catch (const std::exception &e) { reportError("設定の読み込みに失敗: " + QString::fromUtf8(e.what())); }
}
void Window::closeEvent(QCloseEvent *event) {
    if (runner_->running()) {
        if (QMessageBox::question(this, "終了", "実行を中止して終了しますか？") != QMessageBox::Yes) { event->ignore(); return; }
        runner_->cancel();
    }
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
