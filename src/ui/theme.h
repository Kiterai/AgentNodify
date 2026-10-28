#pragma once
#include <QPushButton>
#include <QColor>
#include "core/flow.h"
class QComboBox;

namespace an {
void styleComboBoxPopup(QComboBox *combo);
bool hasSpecialNodeBackground(Kind kind);
QColor nodeBackground(Kind kind);
QColor nodeForeground(Kind kind);
QColor nodeAccent(Kind kind);
class ElidedButton : public QPushButton {
public:
    using QPushButton::QPushButton;
    QString displayedText() const;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
};
}
