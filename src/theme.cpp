#include "theme.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QLineEdit>
#include <QPalette>
#include <QStyledItemDelegate>

namespace an {
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
