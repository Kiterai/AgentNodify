#pragma once
#include <QObject>
namespace an {
class ProgressTest : public QObject {
    Q_OBJECT
private slots:
    void protocols_data();
    void protocols();
    void claudeStreaming();
    void openCodePatchAndErrors();
    void jsonLimitsAndMarkdown();
    void toolFoldsAndReplay();
    void borderlessLayout();
    void compactJsonTree();
    void roundedCodeBlocks();
    void inlineCodeBackground();
    void disclosureHover();
    void longMarkdownMessage();
    void claudeThinking();
    void executionBoundaries();
    void commandPresentation();
    void commandElisionAndStatus();
    void ignoredControlEvents();
    void rowClickTargets();
    void rowHoverAndIconPlacement();
    void globPresentation();
    void fileToolPresentation();
    void fallbackAndPartialLines();
};
}
