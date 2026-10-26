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
    void fallbackAndPartialLines();
};
}
