#include "nodeform.h"
#include "theme.h"
#include <QComboBox>
#include <QCheckBox>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyle>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace an {
namespace {
class NodeCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *event) override {
        QCheckBox::paintEvent(event);
        // Native Windows checkbox indicators can remain black despite QSS text
        // colors. Paint the indicator explicitly, including read-only forms.
        QStyleOptionButton option; initStyleOption(&option);
        const auto rect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, this);
        QPainter painter(this); painter.fillRect(rect, QColor("#17202e")); painter.setRenderHint(QPainter::Antialiasing);
        const QColor white("#e6edf8"); painter.setPen(QPen(white, 1.4)); painter.setBrush(QColor("#17202e"));
        painter.drawRoundedRect(QRectF(rect).adjusted(1, 1, -1, -1), 2, 2);
        if (isChecked()) {
            painter.setPen(QPen(white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const QPointF points[] = {{rect.left() + rect.width() * .23, rect.top() + rect.height() * .50},
                {rect.left() + rect.width() * .43, rect.top() + rect.height() * .72},
                {rect.left() + rect.width() * .78, rect.top() + rect.height() * .27}};
            painter.drawPolyline(points, 3);
        }
    }
};
class ModelCombo : public QComboBox {
public:
    std::function<void()> beforePopup;
    void showPopup() override { if (beforePopup) beforePopup(); QComboBox::showPopup(); }
};
QWidget *dialogOwner(QWidget *widget) {
    if (auto *proxy = widget->graphicsProxyWidget(); proxy && proxy->scene() && !proxy->scene()->views().isEmpty())
        return proxy->scene()->views().first()->window();
    return widget->window();
}
}
QString nodeDescription(Kind kind) {
    switch (kind) {
    case Kind::Start: return "フローの処理を開始します。開始時に空のオブジェクト {} を1回出力します。";
    case Kind::End: return "フローの処理の終わりを表します。前段から受け取ったデータを最終出力にします。";
    case Kind::ProjectCreate: return "指定フローにプロジェクトを登録し、必要なディレクトリを作成します。出力はresult: success / failedとpathまたはreasonです。同じフロー・パスの重複は失敗します。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
    case Kind::FlowRun: return "指定フロー・プロジェクトを非同期に開始します。出力はresult: started / already / failedです。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
    case Kind::FlowWait: return "指定フロー・プロジェクトの実行終了を待ち、終端の入力を出力します。完了済みなら直ちに返します。未開始・失敗・中止・循環待機はエラーになります。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
    case Kind::Agent: return "選択したエージェントへ1タスクを渡します。入力全体は ${#}、個別の値は ${name}、${obj.key}、${items[0]} で参照できます。${#}はスキーマ未指定の文字列入力でも使用できます。入力を受けるたびに実行します。出力スキーマは省略可能です。";
    case Kind::Script: return "PythonまたはJavaScriptファイルをシステムのPython／Node.jsで実行します。前段データをUTF-8のJSONで標準入力へ渡し、標準出力を次段へ渡します（JSON、または文字列）。相対パスはプロジェクト基準です。実行ディレクトリの省略時はスクリプトのある場所を使います。";
    case Kind::Shell: return "シェルコマンドを実行します（Windows: PowerShell、その他: /bin/sh）。前段の値を ${name} 等でそのまま置換します。出力は exit_code・stdout・stderr を持つJSONです。実行ディレクトリの省略時・相対パスの基準はフローの保存先です。信頼できる入力だけを使用してください。";
    case Kind::All: return "全入力の発火を待ち、入力ポート番号順の配列を出力します。発火後は待ち合わせ状態をリセットし、再び新しい全入力を待ちます。先着した入力はFIFOで保持します。入力数を減らすと、削除した入力端への接続だけを消します。";
    case Kind::Any: return "いずれかの入力にイベントが届くたびに発火します。出力はevent（入力の0始まりインデックス）とinput（その入力データ）です。複数イベントは到着順に1件ずつ処理します。入力数を減らすと、削除した入力端への接続だけを消します。";
    case Kind::UserInput: return "作業画面の中央下部でユーザー入力を待ちます。前段データをメッセージとして表示し、選択したボタンの出力端だけへ入力文字列を渡します。選択肢は最低1個です。選択肢の追加・削除でも残った端への接続を維持します。";
    case Kind::Branch: return "上から順に条件を評価し、最初に一致した出力へ伝播します。一致しない場合はelseへ進みます。比較は文字列で行います。条件の追加・削除でも残った条件と「それ以外」への接続を維持します。";
    }
    return {};
}

TextPreview::TextPreview(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true); setFocusPolicy(Qt::StrongFocus);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(QFont("Consolas", 10));
    setToolTip("クリック、またはEnterで右ペインの編集UIを開きます。");
    connect(this, &QPlainTextEdit::textChanged, this, &TextPreview::resizeToText);
    resizeToText();
}
void TextPreview::mousePressEvent(QMouseEvent *event) {
    QPlainTextEdit::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) emit editRequested();
}
void TextPreview::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        emit editRequested(); event->accept();
    } else QPlainTextEdit::keyPressEvent(event);
}
void TextPreview::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event); resizeToText();
}
void TextPreview::resizeToText() {
    const auto width = qMax(30, viewport()->width() - int(document()->documentMargin() * 2));
    int lines = 0;
    for (auto block = document()->begin(); block.isValid() && lines <= 8; block = block.next()) {
        QTextLayout layout(block.text(), font());
        QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); layout.setTextOption(option);
        layout.beginLayout(); int blockLines = 0;
        while (blockLines + lines <= 8) {
            auto line = layout.createLine(); if (!line.isValid()) break;
            line.setLineWidth(width); ++blockLines;
        }
        layout.endLayout(); lines += qMax(1, blockLines);
    }
    lines_ = qBound(1, lines, 8);
    setVerticalScrollBarPolicy(lines <= 8 ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    // QPlainTextEdit needs a small bottom allowance in addition to its document
    // margins; an exact line-height fit otherwise enables a spurious scrollbar.
    const int chrome = height() - viewport()->height();
    const int height = lines_ * qMax(fontMetrics().height(), fontMetrics().lineSpacing()) + int(std::ceil(document()->documentMargin() * 2)) + qMax(2 * frameWidth(), chrome) + 4;
    if (minimumHeight() != height || maximumHeight() != height) { setFixedHeight(height); emit heightChanged(); }
}

NodeForm::NodeForm(const Node &node, QWidget *parent, ModelCatalog *catalog) : QWidget(parent), node_(node), catalog_(catalog) {
    setObjectName("nodeForm"); setFixedWidth(320);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip(nodeDescription(node.kind));
    setStyleSheet(R"(
        QWidget#nodeForm { background: transparent; color:#e6edf8; }
        QWidget#nodeForm QWidget { background: transparent; }
        QWidget#nodeForm QLabel { background: transparent; color:#b8c8df; }
        QWidget#nodeForm QCheckBox, QWidget#nodeForm QCheckBox:disabled { color:#e6edf8; spacing:6px; }
        QWidget#nodeForm QCheckBox::indicator { width:16px; height:16px; }
        QWidget#nodeForm QLineEdit, QWidget#nodeForm QComboBox, QWidget#nodeForm QSpinBox,
        QWidget#nodeForm QPlainTextEdit { background:#17202e; color:#e6edf8; border:1px solid #46566e; border-radius:4px; padding:5px; selection-background-color:#3b5680; }
        QWidget#nodeForm QPushButton { background:#314159; color:#dce5f3; border:1px solid #4b5d79; border-radius:4px; padding:4px 8px; }
        QWidget#nodeForm QPushButton:hover { background:#455d7f; }
    )");
    if (node.kind == Kind::UserInput) setStyleSheet(styleSheet() + "QWidget#nodeForm, QWidget#nodeForm QLabel { color:#40242c; }");
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(8, 8, 8, 8); layout->setSpacing(6);
    auto *form = new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    title_ = new QLineEdit(node.title); title_->setObjectName("nodeTitle"); form->addRow("名前", title_);
    connect(title_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.title = text; publish(); });
    layout->addLayout(form);
    auto addPreview = [this, layout](const QString &label, const QString &field, const QString &text) {
        layout->addWidget(new QLabel(label));
        auto *preview = new TextPreview; preview->setObjectName(field + "Preview");
        preview->setPlaceholderText(field == "prompt" ? "クリックして入力プロンプトを編集" : field == "shellCommand" ? "クリックしてシェルコマンドを編集" : "任意 · クリックしてJSON Schemaを編集");
        preview->setPlainText(text); layout->addWidget(preview);
        connect(preview, &TextPreview::editRequested, this, [this, field] { emit textEditRequested(field); });
        connect(preview, &TextPreview::heightChanged, this, &NodeForm::scheduleResize); return preview;
    };
    if (node.kind == Kind::Agent) {
        agent_ = new QComboBox; agent_->setObjectName("nodeAgent"); agent_->addItems({"Codex", "Claude Code", "OpenCode"}); agent_->setCurrentText(node.agent);
        auto *combo = new ModelCombo; model_ = combo; model_->setObjectName("nodeModel"); model_->setEditable(false);
        model_->setPlaceholderText("モデルを選択");
        model_->setMinimumContentsLength(10); model_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        styleComboBoxPopup(agent_); styleComboBoxPopup(model_);
        auto *modelRow = new QWidget; auto *modelLayout = new QHBoxLayout(modelRow); modelLayout->setContentsMargins(0, 0, 0, 0); modelLayout->setSpacing(4);
        modelLayout->addWidget(model_, 1); refreshModels_ = new QPushButton("↻"); refreshModels_->setObjectName("refreshModels"); refreshModels_->setFixedWidth(30);
        modelLayout->addWidget(refreshModels_);
        modelStatus_ = new QLabel; modelStatus_->setObjectName("modelCatalogStatus");
        form->addRow("エージェント", agent_); form->addRow("モデル", modelRow); form->addRow("", modelStatus_);
        effort_ = new QComboBox; effort_->setObjectName("nodeEffort"); styleComboBoxPopup(effort_);
        effort_->addItem("既定（指定なし）", "");
        populateEfforts();
        effort_->setToolTip("モデルが対応する思考量を選んでください。未指定ならCLI・モデルの既定値を使います。OpenCodeではモデルの指定が必要です。");
        form->addRow("思考量", effort_);
        retainContext_ = new NodeCheckBox("前回のコンテキストを引き継ぐ"); retainContext_->setObjectName("retainContext"); retainContext_->setChecked(node.retainContext);
        retainContext_->setToolTip("同じフロー実行内で、このノードの前回のCLIセッションを再利用します。別ノードや次回のフロー実行とは共有しません。実行履歴は毎回個別に記録します。");
        form->addRow(retainContext_);
        connect(retainContext_, &QCheckBox::toggled, this, [this](bool enabled) { if (!updating_) { node_.retainContext = enabled; publish(); } });
        connect(effort_, &QComboBox::currentIndexChanged, this, [this] { if (!updating_) { node_.effort = effort_->currentData().toString(); publish(); } });
        populateModels();
        combo->beforePopup = [this] { if (catalog_) catalog_->request(node_.agent); };
        connect(refreshModels_, &QPushButton::clicked, this, [this] { if (catalog_) catalog_->request(node_.agent, true); });
        if (catalog_) {
            connect(catalog_, &ModelCatalog::changed, this, [this](const QString &agent) { if (agent == node_.agent) populateModels(); });
            QTimer::singleShot(0, this, [this] { if (catalog_) catalog_->request(node_.agent); });
        }
        connect(agent_, &QComboBox::currentTextChanged, this, [this](const QString &agent) {
            if (updating_) return;
            node_.agent = agent; node_.model.clear(); node_.effort.clear(); effort_->setCurrentIndex(0); populateModels(); publish();
            if (catalog_) catalog_->request(agent);
        });
        connect(model_, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (updating_) return;
            if (index < 0) return;
            node_.model = model_->currentData().toString();
            populateEfforts();
            publish();
        });
        prompt_ = addPreview("入力プロンプト", "prompt", node.prompt);
        schema_ = addPreview("出力スキーマ（任意）", "schema", node.schema);
    } else if (node.kind == Kind::Script) {
        scriptLanguage_ = new QComboBox; scriptLanguage_->setObjectName("scriptLanguage");
        scriptLanguage_->addItem("Python", "python"); scriptLanguage_->addItem("JavaScript (Node.js)", "javascript");
        styleComboBoxPopup(scriptLanguage_);
        if (scriptLanguage_->findData(node.scriptLanguage) < 0) scriptLanguage_->addItem(node.scriptLanguage + "（未対応）", node.scriptLanguage);
        scriptLanguage_->setCurrentIndex(scriptLanguage_->findData(node.scriptLanguage));
        scriptPath_ = new QLineEdit(node.scriptPath); scriptPath_->setObjectName("scriptPath"); scriptPath_->setPlaceholderText("例: scripts/transform.py");
        scriptDirectory_ = new QLineEdit(node.scriptDirectory); scriptDirectory_->setObjectName("scriptDirectory"); scriptDirectory_->setPlaceholderText("省略時: スクリプトのあるディレクトリ");
        scriptPath_->setToolTip("スクリプトファイルの絶対パス、またはプロジェクトからの相対パス");
        scriptDirectory_->setToolTip("絶対パス、またはプロジェクトからの相対パス。空ならスクリプトのあるディレクトリ");
        auto pathRow = [](QLineEdit *edit, QPushButton *browse) {
            auto *row = new QWidget; auto *layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
            edit->setMinimumWidth(0); layout->addWidget(edit, 1); layout->addWidget(browse); return row;
        };
        scriptFileBrowse_ = new QPushButton("参照…"); scriptFileBrowse_->setObjectName("scriptFileBrowse"); scriptFileBrowse_->setToolTip("スクリプトファイルを選択"); scriptFileBrowse_->setAccessibleName("スクリプトファイルを選択");
        scriptDirectoryBrowse_ = new QPushButton("参照…"); scriptDirectoryBrowse_->setObjectName("scriptDirectoryBrowse"); scriptDirectoryBrowse_->setToolTip("実行ディレクトリを選択"); scriptDirectoryBrowse_->setAccessibleName("実行ディレクトリを選択");
        form->addRow("言語", scriptLanguage_); form->addRow("ファイルパス", pathRow(scriptPath_, scriptFileBrowse_)); form->addRow("実行ディレクトリ", pathRow(scriptDirectory_, scriptDirectoryBrowse_));
        connect(scriptLanguage_, &QComboBox::currentIndexChanged, this, [this] { if (!updating_) { node_.scriptLanguage = scriptLanguage_->currentData().toString(); publish(); } });
        connect(scriptPath_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.scriptPath = text; publish(); });
        connect(scriptDirectory_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.scriptDirectory = text; publish(); });
        connect(scriptFileBrowse_, &QPushButton::clicked, this, [this] {
            if (readOnly_) return;
            QFileDialog dialog(dialogOwner(this), "スクリプトファイルを選択"); dialog.setObjectName("scriptFileDialog");
            dialog.setFileMode(QFileDialog::ExistingFile); dialog.setAcceptMode(QFileDialog::AcceptOpen);
            dialog.setNameFilter(node_.scriptLanguage == "javascript" ? "JavaScript (*.js *.mjs *.cjs);;すべてのファイル (*)" : "Python (*.py *.pyw);;すべてのファイル (*)");
            if (!node_.scriptPath.isEmpty() && QFileInfo(node_.scriptPath).exists()) dialog.selectFile(node_.scriptPath);
            if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
            node_.scriptPath = dialog.selectedFiles().first(); scriptPath_->setText(node_.scriptPath); publish();
        });
        connect(scriptDirectoryBrowse_, &QPushButton::clicked, this, [this] {
            if (readOnly_) return;
            QFileDialog dialog(dialogOwner(this), "実行ディレクトリを選択"); dialog.setObjectName("scriptDirectoryDialog");
            dialog.setFileMode(QFileDialog::Directory); dialog.setOption(QFileDialog::ShowDirsOnly); dialog.setAcceptMode(QFileDialog::AcceptOpen);
            if (!node_.scriptDirectory.isEmpty() && QFileInfo(node_.scriptDirectory).isDir()) dialog.setDirectory(node_.scriptDirectory);
            else if (QFileInfo(node_.scriptPath).isFile()) dialog.setDirectory(QFileInfo(node_.scriptPath).absolutePath());
            if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
            node_.scriptDirectory = dialog.selectedFiles().first(); scriptDirectory_->setText(node_.scriptDirectory); publish();
        });
    } else if (node.kind == Kind::Shell) {
        shellDirectory_ = new QLineEdit(node.shellDirectory); shellDirectory_->setObjectName("shellDirectory"); shellDirectory_->setMinimumWidth(0);
        shellDirectory_->setPlaceholderText("省略時: フローの保存ディレクトリ"); shellDirectory_->setToolTip("絶対パス、またはフローの保存先からの相対パス");
        shellDirectoryBrowse_ = new QPushButton("参照…"); shellDirectoryBrowse_->setObjectName("shellDirectoryBrowse"); shellDirectoryBrowse_->setToolTip("シェルの実行ディレクトリを選択");
        auto *row = new QWidget; auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0); rowLayout->setSpacing(4);
        rowLayout->addWidget(shellDirectory_, 1); rowLayout->addWidget(shellDirectoryBrowse_); form->addRow("実行ディレクトリ", row);
        shellCommand_ = addPreview("コマンド内容", "shellCommand", node.shellCommand);
        connect(shellDirectory_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.shellDirectory = text; publish(); });
        connect(shellDirectoryBrowse_, &QPushButton::clicked, this, [this] {
            if (readOnly_) return;
            QFileDialog dialog(dialogOwner(this), "シェルの実行ディレクトリを選択"); dialog.setObjectName("shellDirectoryDialog");
            dialog.setFileMode(QFileDialog::Directory); dialog.setOption(QFileDialog::ShowDirsOnly); dialog.setAcceptMode(QFileDialog::AcceptOpen);
            if (!node_.shellDirectory.isEmpty() && QFileInfo(node_.shellDirectory).isDir()) dialog.setDirectory(node_.shellDirectory);
            if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
            node_.shellDirectory = dialog.selectedFiles().first(); shellDirectory_->setText(node_.shellDirectory); publish();
        });
    } else if (isHostOperation(node.kind)) {
        targetFlow_ = new QComboBox; targetFlow_->setObjectName("targetFlow"); targetFlow_->setPlaceholderText("フローを選択"); styleComboBoxPopup(targetFlow_);
        targetFlow_->setMinimumContentsLength(10); targetFlow_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        projectPath_ = new QLineEdit(node.projectPath); projectPath_->setObjectName("projectPath"); projectPath_->setMinimumWidth(0);
        projectPath_->setPlaceholderText("例: ../projects/${name}"); projectPath_->setToolTip("絶対パス、または実行元プロジェクトからの相対パス。${name}等で前段の値を参照できます。");
        form->addRow("フロー", targetFlow_); form->addRow("プロジェクトパス", projectPath_); setFlowChoices({});
        connect(targetFlow_, &QComboBox::currentIndexChanged, this, [this] { if (!updating_) { node_.targetFlow = targetFlow_->currentData().toString(); publish(); } });
        connect(projectPath_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.projectPath = text; publish(); });
    } else if (node.kind == Kind::UserInput) {
        layout->addWidget(new QLabel("ユーザー選択肢"));
        choices_ = new QWidget; choices_->setObjectName("userChoices"); choicesLayout_ = new QVBoxLayout(choices_);
        choicesLayout_->setContentsMargins(0, 0, 0, 0); choicesLayout_->setSpacing(4); layout->addWidget(choices_);
        addChoice_ = new QPushButton("＋ 選択肢"); addChoice_->setObjectName("addChoice"); layout->addWidget(addChoice_);
        connect(addChoice_, &QPushButton::clicked, this, [this] {
            if (readOnly_) return;
            node_.userChoices.push_back(QString("選択肢 %1").arg(node_.userChoices.size() + 1)); rebuildChoices(); publish();
        });
        rebuildChoices();
    } else if (node.kind == Kind::All || node.kind == Kind::Any) {
        inputs_ = new QSpinBox; inputs_->setObjectName("nodeInputs"); inputs_->setRange(1, 256); inputs_->setValue(int(node.inputs));
        inputs_->setToolTip("残った入力端と出力端への接続は維持し、削除した入力端への接続だけを消します。");
        form->addRow("入力ポート数", inputs_);
        connect(inputs_, &QSpinBox::valueChanged, this, [this](int count) { if (!updating_) { node_.inputs = unsigned(count); publish(); } });
    } else if (node.kind == Kind::Branch) {
        rules_ = new QWidget; rules_->setObjectName("nodeRules"); rulesLayout_ = new QVBoxLayout(rules_);
        rulesLayout_->setContentsMargins(0, 0, 0, 0); rulesLayout_->setSpacing(8); layout->addWidget(rules_);
        rebuildRules();
        auto *add = new QPushButton("＋ 条件"); add->setObjectName("addRule"); layout->addWidget(add);
        connect(add, &QPushButton::clicked, this, [this] { node_.rules.push_back({"", "", true}); rebuildRules(); publish(); });
    }
    for (auto *widget : findChildren<QWidget *>())
        if (qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget) || qobject_cast<QSpinBox *>(widget) || qobject_cast<QCheckBox *>(widget)) widget->installEventFilter(this);
    layout->activate(); setFixedHeight(layout->sizeHint().height()); scheduleResize();
}
bool NodeForm::event(QEvent *event) {
    const bool handled = QWidget::event(event);
    // Newly added choice rows can receive their final stylesheet/font metrics
    // after the initial fixed height was computed. Re-fit instead of clipping.
    if (layout() && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)) scheduleResize();
    return handled;
}
bool NodeForm::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange || event->type() == QEvent::Polish) scheduleResize();
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) emit activated();
    const bool preview = qobject_cast<TextPreview *>(object) || (prompt_ && object == prompt_->viewport()) || (schema_ && object == schema_->viewport()) || (shellCommand_ && object == shellCommand_->viewport());
    if (readOnly_ && !preview &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::KeyPress || event->type() == QEvent::Wheel || event->type() == QEvent::InputMethod)) return true;
    return QWidget::eventFilter(object, event);
}
void NodeForm::setReadOnly(bool readOnly) {
    readOnly_ = readOnly;
    if (retainContext_) retainContext_->setEnabled(!readOnly);
    if (addChoice_) addChoice_->setEnabled(!readOnly);
    if (choices_) for (auto *remove : choices_->findChildren<QPushButton *>("removeChoice")) remove->setEnabled(!readOnly && node_.userChoices.size() > 1);
    if (scriptFileBrowse_) scriptFileBrowse_->setEnabled(!readOnly);
    if (scriptDirectoryBrowse_) scriptDirectoryBrowse_->setEnabled(!readOnly);
    if (shellDirectoryBrowse_) shellDirectoryBrowse_->setEnabled(!readOnly);
    for (auto *widget : findChildren<QWidget *>()) {
        widget->installEventFilter(this);
        if (auto *edit = qobject_cast<QLineEdit *>(widget)) edit->setReadOnly(readOnly);
    }
}
void NodeForm::setFlowChoices(const std::vector<std::pair<QString, QString>> &choices) {
    flowChoices_ = choices;
    if (!targetFlow_) return;
    QSignalBlocker block(targetFlow_); targetFlow_->clear();
    for (const auto &[path, name] : choices) { targetFlow_->addItem(name, path); targetFlow_->setItemData(targetFlow_->count() - 1, path, Qt::ToolTipRole); }
    auto index = targetFlow_->findData(node_.targetFlow);
    if (index < 0 && !node_.targetFlow.isEmpty()) { targetFlow_->addItem(node_.targetFlow + "（未登録）", node_.targetFlow); index = targetFlow_->count() - 1; }
    targetFlow_->setCurrentIndex(index);
}
void NodeForm::populateModels() {
    if (!model_) return;
    QSignalBlocker block(model_); model_->clear();
    const auto entry = catalog_ ? catalog_->entry(node_.agent) : CatalogEntry{};
    for (auto &info : entry.models) {
        const auto label = info.name;
        model_->addItem(label, info.id);
        model_->setItemData(model_->count() - 1, info.id + "\n" + info.description, Qt::ToolTipRole);
    }
    auto selected = model_->findData(node_.model);
    if (selected < 0 && !node_.model.isEmpty()) { model_->addItem(node_.model + "（現在の設定）", node_.model); selected = model_->count() - 1; }
    model_->setCurrentIndex(selected);
    QString status = "未取得";
    QString tooltip = "取得した候補からモデルを選択してください。";
    if (entry.state == CatalogState::Loading) status = "取得中…";
    else if (entry.state == CatalogState::Ready) status = QString("%1件のモデル").arg(entry.models.size());
    else if (entry.state == CatalogState::Error) {
        status = entry.models.empty() ? "取得失敗" : "取得失敗 · 前回の候補を表示";
        tooltip = entry.error + "\n" + tooltip;
    }
    modelStatus_->setText(status); modelStatus_->setToolTip(tooltip); model_->setToolTip(tooltip);
    refreshModels_->setEnabled(catalog_ && entry.state != CatalogState::Loading);
    refreshModels_->setToolTip("モデル一覧を再取得\n" + tooltip); populateEfforts(); scheduleResize();
}
void NodeForm::populateEfforts() {
    if (!effort_) return;
    QSignalBlocker blocker(effort_); effort_->clear(); effort_->addItem("既定（指定なし）", "");
    QStringList levels = node_.agent == "Claude Code" ? QStringList{"low", "medium", "high", "xhigh", "max"} : QStringList{"low", "medium", "high", "xhigh"};
    if (catalog_) for (const auto &info : catalog_->entry(node_.agent).models)
        if (info.id == node_.model && !info.efforts.isEmpty()) { levels = info.efforts; break; }
    for (const auto &value : levels) {
        QString label = value; if (!label.isEmpty()) label[0] = label[0].toUpper();
        effort_->addItem(label, value);
    }
    if (effort_->findData(node_.effort) < 0) effort_->addItem(node_.effort + "（現在の設定）", node_.effort);
    effort_->setCurrentIndex(effort_->findData(node_.effort));
}
void NodeForm::publish() { if (!updating_) { emit edited(); scheduleResize(); } }
void NodeForm::setNode(const Node &node) {
    const bool choicesChanged = node_.userChoices != node.userChoices;
    const bool modelChanged = node_.agent != node.agent || node_.model != node.model;
    bool rulesChanged = node_.rules.size() != node.rules.size();
    if (!rulesChanged) for (size_t i = 0; i < node.rules.size(); ++i) {
        auto &a = node_.rules[i]; auto &b = node.rules[i];
        if (a.variable != b.variable || a.value != b.value || a.equals != b.equals) rulesChanged = true;
    }
    updating_ = true; node_ = node;
    if (retainContext_) retainContext_->setChecked(node.retainContext);
    if (title_->text() != node.title) title_->setText(node.title);
    if (agent_) { agent_->setCurrentText(node.agent); if (modelChanged) populateModels(); }
    if (prompt_ && prompt_->toPlainText() != node.prompt) prompt_->setPlainText(node.prompt);
    if (schema_ && schema_->toPlainText() != node.schema) schema_->setPlainText(node.schema);
    if (inputs_) inputs_->setValue(int(node.inputs));
    if (scriptLanguage_) {
        if (scriptLanguage_->findData(node.scriptLanguage) < 0) scriptLanguage_->addItem(node.scriptLanguage + "（未対応）", node.scriptLanguage);
        scriptLanguage_->setCurrentIndex(scriptLanguage_->findData(node.scriptLanguage));
    }
    if (scriptPath_ && scriptPath_->text() != node.scriptPath) scriptPath_->setText(node.scriptPath);
    if (scriptDirectory_ && scriptDirectory_->text() != node.scriptDirectory) scriptDirectory_->setText(node.scriptDirectory);
    if (shellDirectory_ && shellDirectory_->text() != node.shellDirectory) shellDirectory_->setText(node.shellDirectory);
    if (targetFlow_) setFlowChoices(flowChoices_);
    if (projectPath_ && projectPath_->text() != node.projectPath) projectPath_->setText(node.projectPath);
    if (shellCommand_ && shellCommand_->toPlainText() != node.shellCommand) shellCommand_->setPlainText(node.shellCommand);
    if (effort_) {
        if (effort_->findData(node.effort) < 0) effort_->addItem(node.effort, node.effort);
        effort_->setCurrentIndex(effort_->findData(node.effort));
    }
    if (rules_ && rulesChanged) rebuildRules();
    if (choices_ && choicesChanged) rebuildChoices();
    updating_ = false; scheduleResize();
}
void NodeForm::rebuildChoices() {
    while (auto *item = choicesLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
        delete item;
    }
    for (qsizetype i = 0; i < node_.userChoices.size(); ++i) {
        auto *row = new QWidget; auto *layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
        auto *name = new QLineEdit(node_.userChoices[i]); name->setObjectName("choiceName"); name->setMinimumWidth(0);
        name->setToolTip(QString("出力 %1 の表示名とボタンの文字列").arg(i)); layout->addWidget(name, 1);
        auto *remove = new QPushButton("×"); remove->setObjectName("removeChoice"); remove->setFixedWidth(28); layout->addWidget(remove); choicesLayout_->addWidget(row);
        connect(name, &QLineEdit::textEdited, this, [this, i](const QString &text) { if (!updating_ && !readOnly_ && i < node_.userChoices.size()) { node_.userChoices[i] = text; publish(); } });
        connect(remove, &QPushButton::clicked, this, [this, i] {
            if (readOnly_ || node_.userChoices.size() <= 1 || i >= node_.userChoices.size()) return;
            node_.userChoices.removeAt(i); rebuildChoices(); emit outputRemoved(unsigned(i)); scheduleResize();
        });
    }
    setReadOnly(readOnly_); scheduleResize();
}
void NodeForm::rebuildRules() {
    while (auto *item = rulesLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
        delete item;
    }
    for (size_t i = 0; i < node_.rules.size(); ++i) {
        auto *card = new QWidget; auto *layout = new QVBoxLayout(card); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
        auto *heading = new QHBoxLayout; heading->addWidget(new QLabel(QString("条件%1").arg(i + 1)), 1);
        auto *remove = new QPushButton("×"); remove->setObjectName("removeRule"); remove->setFixedWidth(28); remove->setToolTip("この条件を削除"); heading->addWidget(remove); layout->addLayout(heading);
        auto *variable = new QLineEdit(node_.rules[i].variable); variable->setObjectName("ruleVariable"); variable->setPlaceholderText("前段の変数名");
        auto *subject = new QHBoxLayout; subject->addWidget(new QLabel("もしも")); subject->addWidget(variable, 1); subject->addWidget(new QLabel("が")); layout->addLayout(subject);
        auto *comparison = new QHBoxLayout;
        auto *op = new QComboBox; op->setObjectName("ruleOperator"); op->addItems({"等しい", "異なる"}); op->setCurrentIndex(node_.rules[i].equals ? 0 : 1);
        styleComboBoxPopup(op);
        auto *value = new QLineEdit(node_.rules[i].value); value->setObjectName("ruleValue"); value->setPlaceholderText("比較する値");
        variable->installEventFilter(this); value->installEventFilter(this); op->installEventFilter(this);
        comparison->addWidget(value, 1); comparison->addWidget(new QLabel("と")); comparison->addWidget(op); comparison->addWidget(new QLabel("ならば")); layout->addLayout(comparison); rulesLayout_->addWidget(card);
        connect(variable, &QLineEdit::textEdited, card, [this, i](const QString &text) { if (i < node_.rules.size()) { node_.rules[i].variable = text; publish(); } });
        connect(value, &QLineEdit::textEdited, card, [this, i](const QString &text) { if (i < node_.rules.size()) { node_.rules[i].value = text; publish(); } });
        connect(op, &QComboBox::currentIndexChanged, card, [this, i](int index) { if (i < node_.rules.size()) { node_.rules[i].equals = index == 0; publish(); } });
        connect(remove, &QPushButton::clicked, card, [this, i] {
            if (readOnly_ || i >= node_.rules.size()) return;
            node_.rules.erase(node_.rules.begin() + i); rebuildRules(); emit outputRemoved(unsigned(i)); scheduleResize();
        });
    }
    rulesLayout_->addWidget(new QLabel("それ以外")); scheduleResize();
}
void NodeForm::scheduleResize() {
    if (resizeScheduled_) return; resizeScheduled_ = true;
    QTimer::singleShot(0, this, [this] {
        resizeScheduled_ = false; layout()->activate();
        const auto height = layout()->sizeHint().height();
        if (height != this->height()) { setFixedHeight(height); emit sizeChanged(); }
    });
}
}
