#include "progressview.h"
#include <QAbstractTextDocumentLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QTextBlock>
#include <QTextLayout>
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
const QColor codeBackground("#273449");
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
    auto *item = new QTreeWidgetItem(parent, {name.isEmpty() ? summary(value) : name + ": " + summary(value)}); item->setData(0, Qt::UserRole, path);
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
        setVerticalScrollBarPolicy(!markdown_ && wanted > 420 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        const auto height = markdown_ ? qMax(32, wanted) : qBound(32, wanted, 420);
        if (this->height() != height) setFixedHeight(height);
    }
    void styleMarkdown() {
        markdown_ = true;
        for (auto block = document()->begin(); block.isValid(); block = block.next()) {
            auto format = block.blockFormat();
            format.setLineHeight(115, QTextBlockFormat::ProportionalHeight);
            if (isCode(block)) {
                format.setBackground(Qt::NoBrush);
                format.setLeftMargin(6); format.setRightMargin(6);
                format.setTopMargin(isCode(block.previous()) ? 0 : 14);
                format.setBottomMargin(isCode(block.next()) ? 0 : 14);
            }
            QTextCursor cursor(block); cursor.setBlockFormat(format);
        }
    }
    void setPlainContent(const QString &text) { markdown_ = false; setPlainText(text); }
protected:
    void paintEvent(QPaintEvent *event) override {
        // QTextBrowser draws the text after these backgrounds, preserving selection,
        // copying and its normal Markdown layout. The viewport erases before painting.
        {
            QPainter painter(viewport()); painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen); painter.setBrush(codeBackground);
            const QPointF offset(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
            for (auto block = document()->begin(); markdown_ && block.isValid(); block = block.next()) {
                if (!isCode(block)) { paintInlineCode(painter, block, offset); continue; }
                auto rect = document()->documentLayout()->blockBoundingRect(block);
                while (isCode(block.next())) { block = block.next(); rect = rect.united(document()->documentLayout()->blockBoundingRect(block)); }
                const qreal margin = document()->documentMargin();
                rect.setLeft(margin); rect.setRight(document()->size().width() - margin);
                rect.adjust(0, -10, 0, 10); rect.translate(offset);
                if (rect.intersects(viewport()->rect())) painter.drawRoundedRect(rect, 6, 6);
            }
        }
        QTextBrowser::paintEvent(event);
    }
    QVariant loadResource(int, const QUrl &) override { return {}; } // No local/remote images or resources from agent text.
    void resizeEvent(QResizeEvent *event) override { QTextBrowser::resizeEvent(event); resizeContent(); }
private:
    bool markdown_ = false;
    void paintInlineCode(QPainter &painter, const QTextBlock &block, const QPointF &offset) {
        const auto *layout = block.layout(); if (!layout) return;
        const auto origin = document()->documentLayout()->blockBoundingRect(block).topLeft() + offset;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || !fragment.charFormat().fontFixedPitch()) continue;
            const int start = fragment.position() - block.position(), end = start + fragment.length();
            for (int i = 0; i < layout->lineCount(); ++i) {
                const auto line = layout->lineAt(i);
                const int from = qMax(start, line.textStart()), to = qMin(end, line.textStart() + line.textLength());
                if (from >= to) continue;
                const auto left = line.cursorToX(from), right = line.cursorToX(to);
                QRectF rect(origin.x() + qMin(left, right), origin.y() + line.y(), qAbs(right - left), line.height());
                rect.adjust(-3, -2, 3, 2);
                painter.drawRoundedRect(rect, 3, 3);
            }
        }
    }
    static bool isCode(const QTextBlock &block) {
        if (!block.isValid()) return false;
        const auto format = block.blockFormat();
        return format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage) || format.nonBreakableLines();
    }
};
class Tree : public QTreeWidget {
public:
    explicit Tree(QWidget *parent) : QTreeWidget(parent) {
        setHeaderHidden(true); setColumnCount(1); setUniformRowHeights(true); setTextElideMode(Qt::ElideNone);
        setIndentation(12);
        header()->setSectionResizeMode(QHeaderView::ResizeToContents); header()->setStretchLastSection(false);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(this, &QTreeWidget::itemExpanded, this, [this] { resizeContent(); });
        connect(this, &QTreeWidget::itemCollapsed, this, [this] { resizeContent(); });
        connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this] { QTimer::singleShot(0, this, [this] { resizeContent(); }); });
    }
    void resizeContent() {
        if (resizing_) return;
        QScopedValueRollback<bool> guard(resizing_, true);
        ensurePolished(); doItemsLayout();
        int rows = 0;
        std::function<void(QTreeWidgetItem *)> count = [&](auto *item) { ++rows; if (item->isExpanded()) for (int i = 0; i < item->childCount(); ++i) count(item->child(i)); };
        for (int i = 0; i < topLevelItemCount(); ++i) count(topLevelItem(i));
        const int rowHeight = qMax(1, sizeHintForRow(0));
        const int scrollbar = horizontalScrollBar()->maximum() > 0 ? horizontalScrollBar()->sizeHint().height() : 0;
        const int wanted = qMax(1, rows) * rowHeight + 2 * frameWidth() + scrollbar;
        if (height() != qMin(wanted, 360)) setFixedHeight(qMin(wanted, 360));
    }
protected:
    void resizeEvent(QResizeEvent *event) override { QTreeWidget::resizeEvent(event); resizeContent(); }
private:
    bool resizing_ = false;
};
// Allocate text and icon separately: native RTL tool-button layouts can place
// the arrow over the text when borderless style-sheet padding is zero.
class DisclosureButton : public QToolButton {
public:
    DisclosureButton() { setAttribute(Qt::WA_Hover); }
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 24, qMax(16, fontMetrics().height()) + 4}; }
    QSize minimumSizeHint() const override { return sizeHint(); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this); QStyleOptionToolButton option; initStyleOption(&option);
        if (isEnabled() && (option.state & QStyle::State_MouseOver)) {
            option.palette.setColor(QPalette::ButtonText, QColor("#ffffff"));
            option.palette.setColor(QPalette::WindowText, QColor("#ffffff"));
        }
        const QRect textRect(2, 0, width() - 24, height());
        style()->drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, isEnabled(), text(), QPalette::ButtonText);
        option.rect = QRect(width() - 20, (height() - 10) / 2, 12, 12);
        style()->drawPrimitive(arrowType() == Qt::DownArrow ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowRight, &option, &painter, this);
    }
};
class Detail : public QWidget {
public:
    Detail(const QString &title, const QString &name, QWidget *parent, QLabel *heading = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(4);
        auto *header = new QHBoxLayout; header->setContentsMargins(0, 0, 0, 0); header->setSpacing(8);
        if (heading) { heading->setWordWrap(false); header->addWidget(heading); }
        toggle_ = new DisclosureButton; toggle_->setText(title); toggle_->setObjectName(name);
        toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); toggle_->setArrowType(Qt::RightArrow);
        toggle_->setAutoRaise(true); toggle_->setCursor(Qt::PointingHandCursor); toggle_->setCheckable(true);
        header->addWidget(toggle_, 0, Qt::AlignLeft); header->addStretch(); layout->addLayout(header);
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
    setStyleSheet("QWidget#contentView { background:transparent; color:#dce5f3; } QTextBrowser, QTreeWidget { background:#121925; color:#dce5f3; border:1px solid #354259; padding:4px; selection-background-color:#3b5680; selection-color:white; } QWidget#progressCard QTextBrowser, QWidget#progressCard QTreeWidget { background:transparent; border:0; padding:0; } QTreeWidget::item:selected { background:#3b5680; color:white; }");
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
        addTree(tree_->invisibleRootItem(), {}, parsed, "", 0, remaining);
        if (parsed.is_structured() && !parsed.empty()) {
            auto *root = tree_->takeTopLevelItem(0);
            tree_->addTopLevelItems(root->takeChildren()); delete root;
        }
        std::function<void(QTreeWidgetItem *)> restore = [&](auto *item) {
            item->setExpanded(expanded.contains(item->data(0, Qt::UserRole).toString()) || ((first || empty) && item->parent() == nullptr));
            for (int i = 0; i < item->childCount(); ++i) restore(item->child(i));
        };
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) restore(tree_->topLevelItem(i));
        static_cast<Tree *>(tree_)->resizeContent(); browser_->hide(); tree_->show(); setFocusProxy(tree_);
    } else {
        tree_->hide(); browser_->show(); setFocusProxy(browser_);
        if (markdown) {
            browser_->document()->setMarkdown(plain_, QTextDocument::MarkdownFeatures::fromInt(QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
            static_cast<Browser *>(browser_)->styleMarkdown();
        }
        else static_cast<Browser *>(browser_)->setPlainContent(plain_);
        static_cast<Browser *>(browser_)->resizeContent();
    }
}
class ProgressView::Card : public QWidget {
public:
    Card(ProgressKind kind, QWidget *parent) : QWidget(parent), kind_(kind) {
        setObjectName("progressCard"); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        setStyleSheet("QWidget#progressCard { background:transparent; border:0; } QLabel { color:#dce5f3; background:transparent; border:none; } QToolButton { color:#aebed5; background:transparent; border:0; padding:0; } QToolButton:hover { color:#ffffff; } QToolButton:focus { color:#9bc7ff; }");
        auto *layout = new QVBoxLayout(this); layout->setContentsMargins(4, 2, 4, 2); layout->setSpacing(2);
        if (kind != ProgressKind::Message) {
            heading_ = new QLabel; heading_->setObjectName("progressHeading"); heading_->setTextFormat(Qt::PlainText); heading_->setWordWrap(true);
            auto font = heading_->font(); font.setBold(true); heading_->setFont(font);
            if (kind != ProgressKind::Simple) layout->addWidget(heading_);
        }
        if (kind == ProgressKind::Message) { content_ = new ContentView(this); layout->addWidget(content_); }
        else if (kind == ProgressKind::Tool) {
            input_ = new Detail("入力", "toolInputToggle", this); output_ = new Detail("出力", "toolOutputToggle", this); layout->addWidget(input_); layout->addWidget(output_);
            input_->layout()->setContentsMargins(12, 0, 0, 0); output_->layout()->setContentsMargins(12, 0, 0, 0);
        } else if (kind == ProgressKind::Simple) { details_ = new Detail("詳細", "eventDetailToggle", this, heading_); layout->addWidget(details_); }
    }
    void update(const ProgressItem &item) {
        if (kind_ == ProgressKind::Message) { content_->setValue(item.content, true, item.jsonContent); }
        else if (kind_ == ProgressKind::Tool) {
            heading_->setText(item.name + " · " + progressStatus(item.status));
            input_->setValue(item.input, item.hasInput); output_->setValue(item.output, item.hasOutput);
        } else if (kind_ == ProgressKind::FileAccess) {
            const auto name = item.path.section('/', -1); heading_->setText(item.name + " · " + (name.isEmpty() ? item.path : name) + " · " + progressStatus(item.status)); heading_->setToolTip("<qt>" + item.path.toHtmlEscaped() + "</qt>");
        } else {
            heading_->setText(item.name + (item.name.isEmpty() || item.text.isEmpty() ? "" : " · ") + shortString(item.text));
            details_->findChild<QToolButton *>("eventDetailToggle")->setVisible(!item.content.is_null()); details_->setValue(item.content, !item.content.is_null());
        }
    }
private:
    ProgressKind kind_;
    QLabel *heading_ = nullptr;
    ContentView *content_ = nullptr;
    Detail *input_ = nullptr, *output_ = nullptr, *details_ = nullptr;
};
ProgressView::ProgressView(QWidget *parent) : QScrollArea(parent) {
    setObjectName("progressTimeline"); setWidgetResizable(true); setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("QScrollArea#progressTimeline, QWidget#progressItems { background:#191f2a; color:#dce5f3; }");
    setFocusPolicy(Qt::StrongFocus);
    auto *content = new QWidget; content->setObjectName("progressItems"); layout_ = new QVBoxLayout(content); layout_->setContentsMargins(2, 2, 2, 2); layout_->setSpacing(3); layout_->addStretch(); setWidget(content);
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
void ProgressView::setProgress(const QString &log, const QString &executionKey, State state, const Json *finalOutput, bool structuredOutput, const QString &directory) {
    const bool changed = key_ != executionKey || !log.startsWith(raw_);
    const auto scroll = verticalScrollBar()->value(); QScopedValueRollback<bool> guard(adjustingScroll_, true);
    if (changed) { followBottom_ = true; reset(); key_ = executionKey; }
    parser_.setDirectory(directory);
    parser_.append(log.mid(raw_.size())); raw_ = log;
    if (state != State::Running && state != State::Pending) parser_.finish(state);
    if (finalOutput) parser_.finalResponse(*finalOutput, structuredOutput);
    for (const auto &entry : parser_.items()) {
        if (entry.finalResponse) { delete cards_.take(entry.key); continue; }
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
