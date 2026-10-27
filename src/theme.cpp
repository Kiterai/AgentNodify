#include "theme.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QPalette>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QStylePainter>

namespace an {
QString ElidedButton::displayedText() const {
    QStyleOptionButton option; initStyleOption(&option);
    int available = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this).width();
    if (!icon().isNull()) available -= iconSize().width() + 4;
    if (fontMetrics().horizontalAdvance(text()) <= available) return text();
    const auto width = qMax(0, available - fontMetrics().horizontalAdvance("...") + fontMetrics().horizontalAdvance(QChar(0x2026)));
    return fontMetrics().elidedText(text(), Qt::ElideRight, width).replace(QChar(0x2026), "...");
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
        const QColor text(group == QPalette::Disabled ? "#9caec7" : "#e6edf8");
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::Base, QColor("#17202e"));
        palette.setColor(group, QPalette::Window, QColor("#17202e"));
        palette.setColor(group, QPalette::Highlight, QColor("#3b5680"));
        palette.setColor(group, QPalette::HighlightedText, QColor("#ffffff"));
    }
    view->setPalette(palette); view->viewport()->setPalette(palette);
    view->setStyleSheet(R"(
        QAbstractItemView { background-color:#17202e; color:#e6edf8;
            border:1px solid #46566e; selection-background-color:#3b5680;
            selection-color:#ffffff; outline:0; }
        QAbstractItemView::item { color:#e6edf8; padding:5px 8px; min-height:20px; }
        QAbstractItemView::item:hover, QAbstractItemView::item:selected {
            background-color:#3b5680; color:#ffffff; }
        QAbstractItemView:disabled, QAbstractItemView::item:disabled { color:#9caec7; }
    )");
}
}
