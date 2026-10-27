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
    void paletteWidthAndElision();
    void editorUndoRedoParameters();
    void editorUndoRedoDeletion();
    void editorUndoIsolationAndPorts();
    void widgetLifetime();
    void comboPopupColors();
    void workSelection();
    void flowRemoval();
    void nodeContextAndMenuSpacing();
    void embeddedComboCursor();
    void projectOpeners();
    void comboPopupWheel();
    void nodeExecutionHistory();
    void historyLabelFormat();
    void compactNamelessNodes();
    void runningFlowEditRestrictions();
    void startAndStopDialogs();
    void addedNodeAutoFit();
    void nodePlacementFromLastInteraction();
    void userInputControlEnter();
    void historyAfterRestart();
    void scriptNodeForm();
    void scriptPathDialogs();
    void shellNodeForm();
    void simplifiedCliSettings();
    void spaceZoomToFit();
    void spaceFitFocusAndEmpty();
    void orchestrationForms();
    void orchestrationHistoryAndPersistence();
    void trayVisibilityAndIdleExit();
    void trayFlowContinues();
    void trayQuitWithRunningFlows_data();
    void trayQuitWithRunningFlows();
    void trayExitSavesChanges();
    void allAndAnyForms();
    void agentContextSetting();
    void contextCheckboxColors();
    void userInputNodeForm();
    void userInputWorkPanel();
    void userInputParallelProjects();
    void scopedInputSelectionAndStop();
    void projectStatusIndicators();
    void createdProjectStatusIcons();
    void appendedOutputsPreservePaths();
    void appendedBranchExecutesElse();
    void removedOutputsPreservePaths_data();
    void removedOutputsPreservePaths();
    void bulkPortReduction();
    void reducedUserInputExecutesSurvivingPath();
    void interactionWhileWaiting();
    void removeProjectsWhileWaiting();
};
}
