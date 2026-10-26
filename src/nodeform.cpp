#include "nodeform.h"
#include <QComboBox>
#include <QFormLayout>
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

namespace an {
QString nodeDescription(Kind kind) {
    switch (kind) {
    case Kind::Start: return "フローの処理を開始します。開始時に空のオブジェクト {} を1回出力します。";
    case Kind::End: return "フローの処理の終わりを表します。前段から受け取ったデータを最終出力にします。";
    case Kind::Agent: return "選択したエージェントへ1タスクを渡します。前段の値は ${name}、${obj.key}、${items[0]} で参照できます。入力を受けるたびに実行します。出力スキーマは省略可能です。";
    case Kind::And: return "全入力の発火を待ち、入力ポート番号順の配列を出力します。発火後は待ち合わせ状態をリセットし、再び新しい全入力を待ちます。先着した入力はFIFOで保持します。ポート数の変更時は再接続が必要です。";
    case Kind::Branch: return "上から順に条件を評価し、最初に一致した出力へ伝播します。一致しない場合はelseへ進みます。比較は文字列で行います。条件数の変更時は再接続が必要です。";
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
    for (auto block = document()->begin(); block.isValid() && lines < 8; block = block.next()) {
        QTextLayout layout(block.text(), font());
        QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); layout.setTextOption(option);
        layout.beginLayout(); int blockLines = 0;
        while (blockLines + lines < 8) {
            auto line = layout.createLine(); if (!line.isValid()) break;
            line.setLineWidth(width); ++blockLines;
        }
        layout.endLayout(); lines += qMax(1, blockLines);
    }
    lines_ = qBound(1, lines, 8);
    // QPlainTextEdit needs a small bottom allowance in addition to its document
    // margins; an exact line-height fit otherwise enables a spurious scrollbar.
    const int height = lines_ * qMax(fontMetrics().height(), fontMetrics().lineSpacing()) + int(std::ceil(document()->documentMargin() * 2)) + 2 * frameWidth() + 2;
    if (minimumHeight() != height || maximumHeight() != height) { setFixedHeight(height); emit heightChanged(); }
}

NodeForm::NodeForm(const Node &node, QWidget *parent) : QWidget(parent), node_(node) {
    setObjectName("nodeForm"); setFixedWidth(320);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip(nodeDescription(node.kind));
    setStyleSheet(R"(
        QWidget#nodeForm { background: transparent; color:#e6edf8; }
        QWidget#nodeForm QWidget { background: transparent; }
        QWidget#nodeForm QLabel { background: transparent; color:#b8c8df; }
        QWidget#nodeForm QLineEdit, QWidget#nodeForm QComboBox, QWidget#nodeForm QSpinBox,
        QWidget#nodeForm QPlainTextEdit { background:#17202e; color:#e6edf8; border:1px solid #46566e; border-radius:4px; padding:5px; selection-background-color:#3b5680; }
        QWidget#nodeForm QPushButton { background:#314159; color:#dce5f3; border:1px solid #4b5d79; border-radius:4px; padding:4px 8px; }
        QWidget#nodeForm QPushButton:hover { background:#455d7f; }
    )");
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(8, 8, 8, 8); layout->setSpacing(6);
    auto *form = new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    title_ = new QLineEdit(node.title); title_->setObjectName("nodeTitle"); form->addRow("名前", title_);
    connect(title_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.title = text; publish(); });
    layout->addLayout(form);
    if (node.kind == Kind::Agent) {
        agent_ = new QComboBox; agent_->setObjectName("nodeAgent"); agent_->addItems({"Codex", "Claude Code", "OpenCode"}); agent_->setCurrentText(node.agent);
        model_ = new QComboBox; model_->setObjectName("nodeModel"); model_->setEditable(true);
        model_->setMinimumContentsLength(10); model_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); populateModels();
        model_->setToolTip("モデルIDは編集できます。空欄はCLIの既定モデルです。");
        form->addRow("エージェント", agent_); form->addRow("モデル", model_);
        connect(agent_, &QComboBox::currentTextChanged, this, [this](const QString &agent) {
            if (updating_) return;
            node_.agent = agent; node_.model.clear(); populateModels(); publish();
        });
        connect(model_, &QComboBox::currentTextChanged, this, [this](const QString &model) { if (!updating_) { node_.model = model; publish(); } });
        auto addPreview = [this, layout](const QString &label, const QString &field, const QString &text) {
            layout->addWidget(new QLabel(label));
            auto *preview = new TextPreview; preview->setObjectName(field + "Preview");
            preview->setPlaceholderText(field == "prompt" ? "クリックして入力プロンプトを編集" : "任意 · クリックしてJSON Schemaを編集");
            preview->setPlainText(text); layout->addWidget(preview);
            connect(preview, &TextPreview::editRequested, this, [this, field] { emit textEditRequested(field); });
            connect(preview, &TextPreview::heightChanged, this, &NodeForm::scheduleResize);
            return preview;
        };
        prompt_ = addPreview("入力プロンプト", "prompt", node.prompt);
        schema_ = addPreview("出力スキーマ（任意）", "schema", node.schema);
    } else if (node.kind == Kind::And) {
        inputs_ = new QSpinBox; inputs_->setObjectName("nodeInputs"); inputs_->setRange(1, 256); inputs_->setValue(int(node.inputs));
        inputs_->setToolTip("入力ポート数を変更すると、このノードの接続は解除されます。");
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
        if (qobject_cast<QLineEdit *>(widget) || qobject_cast<QComboBox *>(widget) || qobject_cast<QSpinBox *>(widget)) widget->installEventFilter(this);
    layout->activate(); setFixedHeight(layout->sizeHint().height()); scheduleResize();
}
bool NodeForm::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) emit activated();
    return QWidget::eventFilter(object, event);
}
void NodeForm::populateModels() {
    if (!model_) return;
    QSignalBlocker block(model_); model_->clear(); model_->addItem("");
    if (node_.agent == "Codex") model_->addItems({"gpt-6-luna"});
    else if (node_.agent == "Claude Code") model_->addItems({"haiku", "claude-haiku-5-5"});
    else model_->addItems({"openai/gpt-6-luna", "anthropic/claude-haiku-5-5"});
    model_->setCurrentText(node_.model);
}
void NodeForm::publish() { if (!updating_) { emit edited(); scheduleResize(); } }
void NodeForm::setNode(const Node &node) {
    const bool modelChanged = node_.agent != node.agent || node_.model != node.model;
    bool rulesChanged = node_.rules.size() != node.rules.size();
    if (!rulesChanged) for (size_t i = 0; i < node.rules.size(); ++i) {
        auto &a = node_.rules[i]; auto &b = node.rules[i];
        if (a.variable != b.variable || a.value != b.value || a.equals != b.equals) rulesChanged = true;
    }
    updating_ = true; node_ = node;
    if (title_->text() != node.title) title_->setText(node.title);
    if (agent_) { agent_->setCurrentText(node.agent); if (modelChanged) populateModels(); }
    if (prompt_ && prompt_->toPlainText() != node.prompt) prompt_->setPlainText(node.prompt);
    if (schema_ && schema_->toPlainText() != node.schema) schema_->setPlainText(node.schema);
    if (inputs_) inputs_->setValue(int(node.inputs));
    if (rules_ && rulesChanged) rebuildRules();
    updating_ = false; scheduleResize();
}
void NodeForm::rebuildRules() {
    while (auto *item = rulesLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
        delete item;
    }
    for (size_t i = 0; i < node_.rules.size(); ++i) {
        auto *card = new QWidget; auto *layout = new QVBoxLayout(card); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
        auto *heading = new QHBoxLayout; heading->addWidget(new QLabel(i == 0 ? "if" : "else if"), 1);
        auto *remove = new QPushButton("×"); remove->setObjectName("removeRule"); remove->setFixedWidth(28); remove->setToolTip("この条件を削除"); heading->addWidget(remove); layout->addLayout(heading);
        auto *variable = new QLineEdit(node_.rules[i].variable); variable->setObjectName("ruleVariable"); variable->setPlaceholderText("前段の変数名"); layout->addWidget(variable);
        auto *comparison = new QHBoxLayout;
        auto *op = new QComboBox; op->setObjectName("ruleOperator"); op->addItems({"等しい", "異なる"}); op->setCurrentIndex(node_.rules[i].equals ? 0 : 1);
        auto *value = new QLineEdit(node_.rules[i].value); value->setObjectName("ruleValue"); value->setPlaceholderText("比較する値");
        variable->installEventFilter(this); value->installEventFilter(this); op->installEventFilter(this);
        comparison->addWidget(op); comparison->addWidget(value, 1); layout->addLayout(comparison); rulesLayout_->addWidget(card);
        connect(variable, &QLineEdit::textEdited, card, [this, i](const QString &text) { if (i < node_.rules.size()) { node_.rules[i].variable = text; publish(); } });
        connect(value, &QLineEdit::textEdited, card, [this, i](const QString &text) { if (i < node_.rules.size()) { node_.rules[i].value = text; publish(); } });
        connect(op, &QComboBox::currentIndexChanged, card, [this, i](int index) { if (i < node_.rules.size()) { node_.rules[i].equals = index == 0; publish(); } });
        connect(remove, &QPushButton::clicked, card, [this, i] {
            if (i >= node_.rules.size()) return;
            node_.rules.erase(node_.rules.begin() + i); rebuildRules(); publish();
        });
    }
    rulesLayout_->addWidget(new QLabel("else")); scheduleResize();
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
