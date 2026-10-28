namespace {
class NodeCheckBox : public QCheckBox {
  public:
    using QCheckBox::QCheckBox;

  protected:
    void paintEvent(QPaintEvent *event) override {
        QCheckBox::paintEvent(event);
        // Native Windows checkbox indicators can remain black despite QSS text
        // colors. Paint the indicator explicitly, including read-only forms.
        QStyleOptionButton option;
        initStyleOption(&option);
        const auto rect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, this);
        QPainter painter(this);
        painter.fillRect(rect, QColor(colors::InputBackground));
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor white(colors::InputForeground);
        painter.setPen(QPen(white, metrics::CheckboxStroke));
        painter.setBrush(QColor(colors::InputBackground));
        painter.drawRoundedRect(QRectF(rect).adjusted(1, 1, -1, -1), metrics::CheckboxRadius, metrics::CheckboxRadius);
        if (isChecked()) {
            painter.setPen(QPen(white, metrics::CheckboxTickStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            auto points = metrics::CheckboxTick;
            for (auto &point : points)
                point = QPointF(rect.left() + rect.width() * point.x(), rect.top() + rect.height() * point.y());
            painter.drawPolyline(points.data(), int(points.size()));
        }
    }
};
class ModelCombo : public QComboBox {
  public:
    std::function<void()> beforePopup;
    void showPopup() override {
        if (beforePopup)
            beforePopup();
        QComboBox::showPopup();
    }
};
QWidget *dialogOwner(QWidget *widget) {
    if (auto *proxy = widget->graphicsProxyWidget(); proxy && proxy->scene() && !proxy->scene()->views().isEmpty())
        return proxy->scene()->views().first()->window();
    return widget->window();
}
} // namespace
