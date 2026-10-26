#include "progressview.h"
#include <QAbstractTextDocumentLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QSet>
#include <QScopedValueRollback>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

namespace an {
namespace {
QString shortString(const QString &text) {
    qsizetype characters = 0, end = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i].isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate()) ++i;
        if (++characters == 500) end = i + 1;
    }
    if (characters <= 500) return text;
    return text.left(end) + QString("...以下 %1文字").arg(characters - 500);
}
QString summary(const Json &value) {
    if (value.is_object()) return QString("{%1項目}").arg(value.size());
    if (value.is_array()) return QString("[%1件]").arg(value.size());
    if (value.is_string()) return QString::fromStdString(Json(shortString(QString::fromStdString(value.get<std::string>())).toStdString()).dump());
    return QString::fromStdString(value.dump());
}
void addTree(QTreeWidgetItem *parent, const QString &name, const Json &value, const QString &path, int depth, int &remaining) {
    if (remaining-- <= 0) return;
    auto *item = new QTreeWidgetItem(parent, {name + ": " + summary(value)}); item->setData(0, Qt::UserRole, path);
    if (!value.is_structured()) return;
    if (depth >= 32 || remaining <= 0) { new QTreeWidgetItem(item, {"...表示上限"}); return; }
    if (value.is_array()) {
        const auto count = qMin(size_t(8), value.size());
        size_t shown = 0;
        for (; shown < count && remaining > 0; ++shown) addTree(item, QString("[%1]").arg(shown), value[shown], path + "/" + QString::number(shown), depth + 1, remaining);
        if (shown < count) new QTreeWidgetItem(item, {"...表示上限"});
        if (value.size() > count) new QTreeWidgetItem(item, {QString("...他 %1件の項目").arg(value.size() - count)});
    } else {
        for (auto it = value.begin(); it != value.end() && remaining > 0; ++it) {
            const auto key = QString::fromStdString(it.key()); auto escaped = key; escaped.replace("~", "~0").replace("/", "~1");
            addTree(item, key, it.value(), path + "/" + escaped, depth + 1, remaining);
        }
        if (remaining <= 0) new QTreeWidgetItem(item, {"...表示上限"});
    }
}
class Browser : public QTextBrowser {
public:
    explicit Browser(QWidget *parent) : QTextBrowser(parent) {
        setOpenLinks(false); setOpenExternalLinks(false); setReadOnly(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this] { resizeContent(); });
    }
    void resizeContent() {
        const int wanted = int(std::ceil(document()->documentLayout()->documentSize().height())) + 2 * frameWidth() + 12;
        setVerticalScrollBarPolicy(wanted > 420 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        const auto height = qBound(32, wanted, 420); if (this->height() != height) setFixedHeight(height);
    }
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; } // No local/remote images or resources from agent text.
    void resizeEvent(QResizeEvent *event) override { QTextBrowser::resizeEvent(event); resizeContent(); }
};
class Tree : public QTreeWidget {
public:
    explicit Tree(QWidget *parent) : QTreeWidget(parent) {
        setHeaderHidden(true); setColumnCount(1); setUniformRowHeights(true); setTextElideMode(Qt::ElideNone);
        header()->setSectionResizeMode(QHeaderView::ResizeToContents); header()->setStretchLastSection(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(this, &QTreeWidget::itemExpanded, this, [this] { resizeContent(); });
        connect(this, &QTreeWidget::itemCollapsed, this, [this] { resizeContent(); });
    }
    void resizeContent() {
        int rows = 0;
        std::function<void(QTreeWidgetItem *)> count = [&](auto *item) { ++rows; if (item->isExpanded()) for (int i = 0; i < item->childCount(); ++i) count(item->child(i)); };
        for (int i = 0; i < topLevelItemCount(); ++i) count(topLevelItem(i));
        const auto wanted = qMax(1, rows) * qMax(22, fontMetrics().height() + 6) + 2 * frameWidth() + horizontalScrollBar()->sizeHint().height() + 4;
        setFixedHeight(qBound(40, wanted, 360));
    }
};
class Detail : public QWidget {
public:
    Detail(const QString &title, const QString &name, QWidget *parent) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
        toggle_ = new QToolButton; toggle_->setText(title); toggle_->setObjectName(name); toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toggle_->setCheckable(true); toggle_->setArrowType(Qt::RightArrow); layout->addWidget(toggle_, 0, Qt::AlignLeft);
        connect(toggle_, &QToolButton::toggled, this, [this](bool expanded) {
            toggle_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            if (expanded && !content_) { content_ = new ContentView(this); this->layout()->addWidget(content_); }
            if (content_) { if (expanded) content_->setValue(available_ ? value_ : Json("未取得"), false); content_->setVisible(expanded); }
        });
    }
    void setValue(const Json &value, bool available) {
        value_ = value; available_ = available;
        if (content_ && toggle_->isChecked()) content_->setValue(available ? value : Json("未取得"), false);
    }
private:
    QToolButton *toggle_;
    ContentView *content_ = nullptr;
    Json value_;
    bool available_ = false;
};
}
ContentView::ContentView(QWidget *parent) : QWidget(parent) {
    setObjectName("contentView");
    setStyleSheet("QWidget#contentView { background:transparent; color:#dce5f3; } QTextBrowser, QTreeWidget { background:#121925; color:#dce5f3; border:1px solid #354259; padding:4px; selection-background-color:#3b5680; selection-color:white; } QTreeWidget::item:selected { background:#3b5680; color:white; }");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); setMinimumWidth(0);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    browser_ = new Browser(this); browser_->setObjectName("markdownContent"); layout->addWidget(browser_);
    tree_ = new Tree(this); tree_->setObjectName("jsonContent"); layout->addWidget(tree_); tree_->hide();
    setFocusProxy(browser_);
}
void ContentView::setPlainText(const QString &text) { setValue(Json(text.toStdString())); }
void ContentView::setValue(const Json &value, bool markdown, bool forceJson) {
    if (initialized_ && value_ == value && markdown_ == markdown && forceJson_ == forceJson) return;
    value_ = value; markdown_ = markdown; forceJson_ = forceJson; const bool first = !initialized_; initialized_ = true;
    plain_ = display(value);
    const auto parsed = value.is_string() && !forceJson ? displayJson(QString::fromStdString(value.get<std::string>())) : value;
    if (!parsed.is_discarded()) {
        QSet<QString> expanded;
        std::function<void(QTreeWidgetItem *)> visit = [&](auto *item) {
            if (item->isExpanded()) expanded.insert(item->data(0, Qt::UserRole).toString());
            for (int i = 0; i < item->childCount(); ++i) visit(item->child(i));
        };
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) visit(tree_->topLevelItem(i));
        const bool empty = tree_->topLevelItemCount() == 0;
        tree_->clear(); int remaining = 4096;
        addTree(tree_->invisibleRootItem(), "$", parsed, "", 0, remaining);
        std::function<void(QTreeWidgetItem *)> restore = [&](auto *item) {
            item->setExpanded(expanded.contains(item->data(0, Qt::UserRole).toString()) || ((first || empty) && item->parent() == nullptr));
            for (int i = 0; i < item->childCount(); ++i) restore(item->child(i));
        };
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) restore(tree_->topLevelItem(i));
        static_cast<Tree *>(tree_)->resizeContent(); browser_->hide(); tree_->show(); setFocusProxy(tree_);
    } else {
        tree_->hide(); browser_->show(); setFocusProxy(browser_);
        if (markdown) browser_->document()->setMarkdown(plain_, QTextDocument::MarkdownFeatures::fromInt(QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
        else browser_->setPlainText(plain_);
        static_cast<Browser *>(browser_)->resizeContent();
    }
}
class ProgressView::Card : public QWidget {
public:
    Card(ProgressKind kind, QWidget *parent) : QWidget(parent), kind_(kind) {
        setObjectName("progressCard"); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        setStyleSheet("QWidget#progressCard { background:#202938; border:1px solid #354259; border-radius:5px; } QLabel { color:#dce5f3; background:transparent; border:none; } QToolButton { color:#dce5f3; background:#2d3b50; border:1px solid #40536c; border-radius:3px; padding:3px 6px; }");
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(8, 6, 8, 6); layout->setSpacing(4);
        heading_ = new QLabel; heading_->setObjectName("progressHeading"); heading_->setTextFormat(Qt::PlainText); heading_->setWordWrap(true); layout->addWidget(heading_);
        if (kind == ProgressKind::Message) { content_ = new ContentView(this); layout->addWidget(content_); }
        else if (kind == ProgressKind::Tool) {
            input_ = new Detail("入力", "toolInputToggle", this); output_ = new Detail("出力", "toolOutputToggle", this); layout->addWidget(input_); layout->addWidget(output_);
        } else if (kind == ProgressKind::Simple) { details_ = new Detail("詳細", "eventDetailToggle", this); layout->addWidget(details_); }
    }
    void update(const ProgressItem &item) {
        if (kind_ == ProgressKind::Message) { heading_->setText(item.name); content_->setValue(item.content, true, item.jsonContent); }
        else if (kind_ == ProgressKind::Tool) {
            heading_->setText(item.name + " · " + progressStatus(item.status));
            input_->setValue(item.input, item.hasInput); output_->setValue(item.output, item.hasOutput);
        } else if (kind_ == ProgressKind::FileAccess) {
            const auto name = item.path.section('/', -1); heading_->setText(item.name + " · " + (name.isEmpty() ? item.path : name) + " · " + progressStatus(item.status)); heading_->setToolTip("<qt>" + item.path.toHtmlEscaped() + "</qt>");
        } else {
            heading_->setText(item.name + (item.name.isEmpty() || item.text.isEmpty() ? "" : " · ") + shortString(item.text));
            details_->setVisible(!item.content.is_null()); details_->setValue(item.content, !item.content.is_null());
        }
    }
private:
    ProgressKind kind_;
    QLabel *heading_;
    ContentView *content_ = nullptr;
    Detail *input_ = nullptr, *output_ = nullptr, *details_ = nullptr;
};
ProgressView::ProgressView(QWidget *parent) : QScrollArea(parent) {
    setObjectName("progressTimeline"); setWidgetResizable(true); setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QScrollArea#progressTimeline, QWidget#progressItems { background:#191f2a; color:#dce5f3; }");
    setFocusPolicy(Qt::StrongFocus);
    auto *content = new QWidget; content->setObjectName("progressItems"); layout_ = new QVBoxLayout(content); layout_->setContentsMargins(2, 2, 2, 2); layout_->setSpacing(6); layout_->addStretch(); setWidget(content);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) { if (!adjustingScroll_) followBottom_ = value == verticalScrollBar()->maximum(); });
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int maximum) {
        if (followBottom_) { QScopedValueRollback<bool> guard(adjustingScroll_, true); verticalScrollBar()->setValue(maximum); }
    });
}
void ProgressView::reset() {
    while (layout_->count() > 1) { auto *entry = layout_->takeAt(0); delete entry->widget(); delete entry; }
    cards_.clear(); parser_ = ProgressParser(); raw_.clear();
}
bool ProgressView::finalIsJson() const {
    for (auto it = parser_.items().rbegin(); it != parser_.items().rend(); ++it)
        if (it->kind == ProgressKind::Message && it->name == "最終応答") return it->jsonContent;
    return false;
}
void ProgressView::setProgress(const QString &log, const QString &executionKey, State state, const Json *finalOutput, bool structuredOutput) {
    const bool changed = key_ != executionKey || !log.startsWith(raw_);
    const auto scroll = verticalScrollBar()->value(); QScopedValueRollback<bool> guard(adjustingScroll_, true);
    if (changed) { followBottom_ = true; reset(); key_ = executionKey; }
    parser_.append(log.mid(raw_.size())); raw_ = log;
    if (state != State::Running && state != State::Pending) parser_.finish(state);
    if (finalOutput) parser_.finalResponse(*finalOutput, structuredOutput);
    for (const auto &entry : parser_.items()) {
        auto *card = cards_.value(entry.key, nullptr);
        if (!card) { card = new Card(entry.kind, widget()); cards_[entry.key] = card; layout_->insertWidget(layout_->count() - 1, card); }
        card->update(entry);
    }
    layout_->activate(); const auto revision = ++revision_;
    QTimer::singleShot(0, this, [this, scroll, revision] {
        if (revision != revision_) return;
        QScopedValueRollback<bool> guard(adjustingScroll_, true);
        verticalScrollBar()->setValue(followBottom_ ? verticalScrollBar()->maximum() : scroll);
    });
}
}
