#include "shared/constants.h"
#include "shared/strings.h"
#include "core/nodedefinition.h"
#include "nodepresentation.h"
#include "nodeform.h"
#include "nodeparameterform.h"
#include "theme.h"
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace an {
QString nodeDescription(Kind kind) { return sentenceToolTip(nodePresentation(kind).description); }

TextPreview::TextPreview(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true); setFocusPolicy(Qt::StrongFocus);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(QFont(strings::CodeFont, metrics::CodeFontPoints));
    setToolTip(strings::TextPreviewTooltip);
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
    const auto width = qMax(metrics::PreviewMinimumWidth, viewport()->width() - int(document()->documentMargin() * 2));
    int lines = 0;
    for (auto block = document()->begin(); block.isValid() && lines <= limits::PreviewLines; block = block.next()) {
        QTextLayout layout(block.text(), font());
        QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); layout.setTextOption(option);
        layout.beginLayout(); int blockLines = 0;
        while (blockLines + lines <= limits::PreviewLines) {
            auto line = layout.createLine(); if (!line.isValid()) break;
            line.setLineWidth(width); ++blockLines;
        }
        layout.endLayout(); lines += qMax(1, blockLines);
    }
    lines_ = qBound(1, lines, limits::PreviewLines);
    setVerticalScrollBarPolicy(lines <= limits::PreviewLines ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    // QPlainTextEdit needs a small bottom allowance in addition to its document
    // margins; an exact line-height fit otherwise enables a spurious scrollbar.
    const int chrome = height() - viewport()->height();
    const int height = lines_ * qMax(fontMetrics().height(), fontMetrics().lineSpacing()) + int(std::ceil(document()->documentMargin() * 2)) + qMax(2 * frameWidth(), chrome) + metrics::PreviewBottomPadding;
    if (minimumHeight() != height || maximumHeight() != height) { setFixedHeight(height); emit heightChanged(); }
}

TextPreview *NodeForm::addPreview(QVBoxLayout *layout, const QString &label, const QString &field, const QString &text, const QString &placeholder) {
    layout->addWidget(new QLabel(label));
    auto *preview = new TextPreview;
    preview->setObjectName(field + strings::Preview); preview->setPlaceholderText(placeholder);
    preview->setPlainText(text); layout->addWidget(preview);
    connect(preview, &TextPreview::editRequested, this, [this, field] { emit textEditRequested(field); });
    connect(preview, &TextPreview::heightChanged, this, &NodeForm::scheduleResize);
    return preview;
}
NodeForm::NodeForm(const Node &node, QWidget *parent, ModelCatalog *catalog) : QWidget(parent), node_(node) {
    setObjectName(strings::NodeForm); setFixedWidth(nodePresentation(node.kind).compact ? metrics::CompactNodeWidth : metrics::StandardNodeWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip(nodeDescription(node.kind));
    setStyleSheet(strings::NodeFormStyle);
    if (hasSpecialNodeBackground(node.kind)) setStyleSheet(styleSheet() + QString(strings::NodeFormForegroundStyle).arg(nodeForeground(node.kind).name()));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(metrics::SpaceMedium, metrics::SpaceMedium, metrics::SpaceMedium, metrics::SpaceMedium); layout->setSpacing(metrics::SpaceNormal);
    auto *form = new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    if (hasNodeName(node.kind)) {
        title_ = new QLineEdit(node.title); title_->setMinimumWidth(0); title_->setObjectName(strings::NodeTitle); form->addRow(strings::NameLabel, title_);
        connect(title_, &QLineEdit::textEdited, this, [this](const QString &text) { node_.title = text; publish(); });
    } else node_.title.clear();
    layout->addLayout(form);
    parameters_ = createNodeForm(*this, layout, form, catalog);
    setReadOnly(readOnly_);
    for (auto *widget : findChildren<QWidget *>()) widget->installEventFilter(this);
    layout->activate(); setFixedHeight(layout->sizeHint().height()); scheduleResize();
}
NodeForm::~NodeForm() = default;
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
    const bool preview = qobject_cast<TextPreview *>(object) || qobject_cast<TextPreview *>(object->parent());
    const bool control = preview || qobject_cast<QLineEdit *>(object) || qobject_cast<QComboBox *>(object) || qobject_cast<QAbstractSpinBox *>(object) || qobject_cast<QAbstractButton *>(object);
    const bool editEvent = event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress || event->type() == QEvent::InputMethod ||
        (event->type() == QEvent::Wheel && (qobject_cast<QComboBox *>(object) || qobject_cast<QAbstractSpinBox *>(object)));
    if (!readOnly_ && control && editEvent && beforeEdit && !beforeEdit()) return true;
    if (readOnly_ && !preview &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::KeyPress || event->type() == QEvent::Wheel || event->type() == QEvent::InputMethod)) return true;
    return QWidget::eventFilter(object, event);
}
void NodeForm::setReadOnly(bool readOnly) {
    readOnly_ = readOnly;
    if (parameters_) parameters_->applyReadOnly(readOnly);
    for (auto *widget : findChildren<QWidget *>()) {
        widget->installEventFilter(this);
        if (auto *edit = qobject_cast<QLineEdit *>(widget)) edit->setReadOnly(readOnly);
    }
}
void NodeForm::setFlowChoices(const std::vector<std::pair<QString, QString>> &choices) {
    if (parameters_) parameters_->setFlowChoices(choices);
}
void NodeForm::publish() { if (!updating_) { emit edited(); scheduleResize(); } }
void NodeForm::setNode(const Node &node) {
    const auto previous = node_;
    updating_ = true; node_ = node;
    if (!hasNodeName(node_.kind)) node_.title.clear();
    if (title_ && title_->text() != node.title) title_->setText(node.title);
    if (parameters_) parameters_->synchronize(previous);
    updating_ = false; scheduleResize();
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
