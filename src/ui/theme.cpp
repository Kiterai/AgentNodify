#include "shared/constants.h"
#include "shared/strings.h"
#include "nodepresentation.h"
#include "theme.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QPalette>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QRegularExpression>

namespace an {
QString sentenceToolTip(QString text) {
    return text.replace(QRegularExpression(strings::ToolTipSentencePattern), QStringLiteral("\\1\n")).trimmed();
}
bool hasSpecialNodeBackground(Kind kind) { return nodePresentation(kind).specialBackground; }
QColor nodeBackground(Kind kind) { return QColor(nodePresentation(kind).background); }
QColor nodeForeground(Kind kind) { return QColor(nodePresentation(kind).foreground); }
QColor nodeAccent(Kind kind) { return QColor(nodePresentation(kind).accent); }
QString ElidedButton::displayedText() const {
    QStyleOptionButton option; initStyleOption(&option);
    int available = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this).width();
    if (!icon().isNull()) available -= iconSize().width() + metrics::ButtonIconSpacing;
    if (fontMetrics().horizontalAdvance(text()) <= available) return text();
    const auto width = qMax(0, available - fontMetrics().horizontalAdvance(strings::Ellipsis) + fontMetrics().horizontalAdvance(QChar(strings::EllipsisCharacter)));
    return fontMetrics().elidedText(text(), Qt::ElideRight, width).replace(QChar(strings::EllipsisCharacter), strings::Ellipsis);
}
QSize ElidedButton::minimumSizeHint() const { auto size = QPushButton::minimumSizeHint(); size.setWidth(0); return size; }
void ElidedButton::paintEvent(QPaintEvent *) {
    QStyleOptionButton option; initStyleOption(&option); option.text = displayedText();
    QStylePainter painter(this); painter.drawControl(QStyle::CE_PushButton, option);
}
void styleComboBoxPopup(QComboBox *combo) {
    // Override the graphics view's ScrollHandDrag cursor on embedded controls.
    combo->setCursor(Qt::ArrowCursor);
    if (combo->lineEdit()) combo->lineEdit()->setCursor(Qt::ArrowCursor);
    auto *view = combo->view();
    view->setCursor(Qt::ArrowCursor); view->viewport()->setCursor(Qt::ArrowCursor);
    // Popup windows do not reliably inherit the embedded node's text palette.
    // Use a regular item delegate instead of the platform's native menu one.
    combo->setItemDelegate(new QStyledItemDelegate(view));
    auto palette = view->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const QColor text(group == QPalette::Disabled ? colors::MutedText : colors::InputForeground);
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::Base, QColor(colors::InputBackground));
        palette.setColor(group, QPalette::Window, QColor(colors::InputBackground));
        palette.setColor(group, QPalette::Highlight, QColor(colors::SelectionBackground));
        palette.setColor(group, QPalette::HighlightedText, QColor(colors::White));
    }
    view->setPalette(palette); view->viewport()->setPalette(palette);
    view->setStyleSheet(strings::ComboPopupStyle);
}
}
