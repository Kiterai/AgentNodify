#pragma once
#include <QPushButton>
class QComboBox;

namespace an {
void styleComboBoxPopup(QComboBox *combo);
class ElidedButton : public QPushButton {
public:
    using QPushButton::QPushButton;
    QString displayedText() const;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
};
}
