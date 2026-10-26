#pragma once
#include <QObject>

namespace an {
class GuiTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void previewHeightAndClick();
    void embeddedParameters();
    void branchAndInputForms();
    void terminalAppearance();
    void editorAndFlowRail();
    void widgetLifetime();
    void comboPopupColors();
    void workSelection();
    void flowRemoval();
    void nodeContextAndMenuSpacing();
    void embeddedComboCursor();
    void projectOpeners();
    void comboPopupWheel();
};
}
