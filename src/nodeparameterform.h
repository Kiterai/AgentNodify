#pragma once
#include "flow.h"
#include <QObject>
#include <functional>

class QVBoxLayout;

namespace an {
class NodeForm;
class TextPreview;

// Common edit context only. Concrete forms own their widgets and helpers in
// nodes/<kind>/form.inc; no concrete form declarations are needed here.
class NodeParameterForm : public QObject {
public:
    explicit NodeParameterForm(NodeForm &owner);
    void synchronize(const Node &previous);
    void applyReadOnly(bool readOnly);
    virtual void setFlowChoices(const std::vector<std::pair<QString, QString>> &choices);

protected:
    NodeForm &owner_;
    Node &node_;
    bool &readOnly_, &updating_;
    std::function<void(const Node &)> syncParameters_;
    std::function<void(bool)> readOnlyParameters_;

    void publish();
    void scheduleResize();
    void setReadOnly(bool readOnly);
    void outputRemoved(unsigned index);
    TextPreview *addPreview(QVBoxLayout *, const QString &label, const QString &field, const QString &text, const QString &placeholder);
};
}
