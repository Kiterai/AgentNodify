#pragma once
#include <QtGlobal>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <array>

namespace an::limits {
inline constexpr qsizetype Mebibyte = 1024 * 1024;
inline constexpr int InputPorts = 256;
inline constexpr int WaitDuration = 1000000;
inline constexpr qsizetype LogCharacters = 2 * Mebibyte;
inline constexpr qsizetype ProcessOutputBytes = 32 * Mebibyte;
inline constexpr qsizetype CatalogResponseBytes = 4 * Mebibyte;
inline constexpr int PumpFirings = 128;
inline constexpr int CatalogCacheSeconds = 600;
inline constexpr int CatalogRetryMilliseconds = 30000;
inline constexpr int CatalogTimeoutMilliseconds = 30000;
inline constexpr int ModelIdCharacters = 512;
inline constexpr int ModelNameCharacters = 2000;
inline constexpr int ModelDescriptionCharacters = 4000;
inline constexpr int EffortCharacters = 100;
inline constexpr int CatalogModels = 10000;
inline constexpr int CatalogPageSize = 100;
inline constexpr int CatalogPages = 100;
inline constexpr int PreviewLines = 8;
inline constexpr int JsonStringCharacters = 500;
inline constexpr int JsonArrayItems = 8;
inline constexpr int JsonTreeDepth = 32;
inline constexpr int JsonTreeItems = 4096;
inline constexpr int JsonComparisonDepth = 128;
inline constexpr int ScheduleSearchYears = 400;
} // namespace an::limits
namespace an::time {
inline constexpr qint64 MillisecondsPerSecond = 1000;
inline constexpr qint64 MillisecondsPerMinute = 60 * MillisecondsPerSecond;
inline constexpr qint64 MillisecondsPerHour = 60 * MillisecondsPerMinute;
inline constexpr qint64 MillisecondsPerDay = 24 * MillisecondsPerHour;
inline constexpr int WaitPollMilliseconds = int(MillisecondsPerMinute);
inline constexpr int ProcessKillMilliseconds = 3000;
inline constexpr int CatalogKillMilliseconds = 1000;
inline constexpr int HistorySaveMilliseconds = 1000;
inline constexpr int HistoryRetryMilliseconds = 5000;
inline constexpr int EditMergeMilliseconds = 750;
inline constexpr int AnimationMilliseconds = 180;
inline constexpr int SmokeMilliseconds = 150;
inline constexpr int StatusShortMilliseconds = 5000;
inline constexpr int StatusSavedMilliseconds = 6000;
inline constexpr int StatusInputMilliseconds = 10000;
inline constexpr int StatusLongMilliseconds = 15000;
} // namespace an::time
namespace an::calendar {
inline constexpr int FirstMonth = 1, LastMonth = 12;
inline constexpr int FirstDay = 1, LastDay = 31;
inline constexpr int FirstHour = 0, LastHour = 23;
inline constexpr int FirstMinute = 0, LastMinute = 59;
inline constexpr int FirstWeekday = 1, LastWeekday = 7;
inline constexpr int ValidationYear = 2000;
inline constexpr int OffsetProbeHour = 12;
} // namespace an::calendar
namespace an::defaults {
inline constexpr unsigned InputPorts = 2;
inline constexpr int WaitDuration = 1;
inline constexpr int FlowVersion = 1;
inline constexpr int JsonIndent = 2;
inline constexpr int EditCommandId = 0x414e01;
inline constexpr int RpcMethodNotFound = -32601;
} // namespace an::defaults
namespace an::metrics {
inline constexpr int CompactNodeWidth = 210, StandardNodeWidth = 320;
inline constexpr int WindowWidth = 1440, WindowHeight = 900;
inline constexpr int InitialNodeY = 120, InitialEndX = 600;
inline constexpr int NodeColumns = 4, NodeColumnStep = 220, NodeRowStep = 160;
inline constexpr int FallbackNodeWidth = 180, FallbackNodeHeight = 80;
inline constexpr int RefreshButtonWidth = 30, RemoveButtonWidth = 28;
inline constexpr int WeekdayTableWidth = 132, WeekdayWidth = 18, WeekdayHeight = 20;
inline constexpr int CodeFontPoints = 10;
inline constexpr int EditorCodeFontPoints = 11;
inline constexpr int PreviewMinimumWidth = 30;
inline constexpr int ComboMinimumCharacters = 10;
inline constexpr int PaletteMinimumWidth = 180, WorkGraphMinimumHeight = 120;
inline constexpr int FlowRailWidth = 88, FlowTileExtent = 56, FlowRailMinimumWidth = 70;
inline constexpr int ChoiceScrollMinimumHeight = 42, ChoiceScrollMaximumHeight = 55;
inline constexpr int EditorPaletteWidth = 247, EditorCanvasWidth = 773, EditorTextWidth = 360;
inline constexpr int HistoryPaneHeight = 180, ProgressPaneHeight = 280, OutputPaneHeight = 220;
inline constexpr int ProjectPaneWidth = 230, WorkCanvasWidth = 780, DetailsPaneWidth = 330;
inline constexpr int EnsureVisibleMargin = 30, ActivityIconExtent = 18;
inline constexpr int DefaultInputPaneHeight = 230;
inline constexpr int CliDialogWidth = 650, CliDialogHeight = 250;
inline constexpr int ProjectRowMinimumTextWidth = 40;
inline constexpr int CodeBlockVerticalMargin = 14, CodeBlockBackgroundPadding = 10;
inline constexpr int TreeIndentation = 12, ContentBottomPadding = 12;
inline constexpr int DetailIndentation = 12, CardSpacing = 12;
inline constexpr int DisclosureTextPadding = 24, DisclosureMinimumHeight = 16;
inline constexpr int DisclosureIconOffset = 20, DisclosureIconCenteringHeight = 10, DisclosureIconExtent = 12;
inline constexpr int UserMessageLines = 3;
inline constexpr int SpaceHairline = 1, SpaceTiny = 2, SpaceSmall = 4, SpaceNormal = 6, SpaceMedium = 8, SpaceComfortable = 10,
                     SpaceLarge = 12;
inline constexpr std::array<int, 4> ActivityIconSizes = {18, 24, 36, 48};
inline constexpr std::array<int, 5> ApplicationIconSizes = {16, 24, 32, 48, 64};
inline constexpr qreal ActivityDesignExtent = 20, ApplicationDesignExtent = 32;
inline constexpr qreal ActivityStroke = 1.5, InputMarkStroke = 2, ApplicationStroke = 2;
inline constexpr qreal BubbleRadius = 3, ApplicationRadius = 7, ApplicationNodeRadius = 4;
inline constexpr QRectF InputBubbleRect{2, 2, 16, 13}, ActivityCircleRect{2, 2, 16, 16};
inline constexpr std::array<QLineF, 2> InputBubbleTail = {QLineF(5, 15, 5, 18), QLineF(5, 18, 9, 15)};
inline constexpr QLineF InputMarkLine{10, 5, 10, 9};
inline constexpr QPointF InputMarkDot{10, 12};
inline constexpr std::array<QPointF, 3> PlayPoints = {QPointF(8, 6), QPointF(14, 10), QPointF(8, 14)};
inline constexpr QRectF ApplicationIconRect{0, 0, ApplicationDesignExtent, ApplicationDesignExtent};
inline constexpr std::array<QLineF, 2> ApplicationEdges = {QLineF(8, 16, 24, 8), QLineF(8, 16, 24, 24)};
inline constexpr std::array<QPointF, 3> ApplicationNodes = {QPointF(8, 16), QPointF(24, 8), QPointF(24, 24)};
inline constexpr qreal CheckboxStroke = 1.4, CheckboxTickStroke = 1.8, CheckboxRadius = 2;
inline constexpr std::array<QPointF, 3> CheckboxTick = {QPointF(.23, .50), QPointF(.43, .72), QPointF(.78, .27)};
inline constexpr int RunningAnimationFrames = 4;
inline constexpr float HighlightBoundaryWidth = 4.0f;
inline constexpr int PreviewBottomPadding = 4, DisclosureVerticalPadding = 4;
inline constexpr int CodeBlockHorizontalMargin = 6, CodeBlockRadius = 6, InlineCodeRadius = 3;
inline constexpr int InlineCodeHorizontalPadding = 3, InlineCodeVerticalPadding = 2;
inline constexpr int TimelineSpacing = 3, ButtonIconSpacing = 4, ProjectRowPadding = 4;
inline constexpr int NodeButtonHoverLightnessPercent = 115;
inline constexpr int SplitterHandleWidth = 6, UserInputMinimumHeight = 60, ActivityIndicatorExtent = 20;
inline constexpr int ContentMinimumHeight = 32, ContentMaximumHeight = 420, TreeMaximumHeight = 360;
inline constexpr int MarkdownLineHeightPercent = 115;
inline constexpr double MarkdownLineHeight = MarkdownLineHeightPercent / 100.0;
inline constexpr double NodeZoomMaximum = 1.5;
inline constexpr double WheelZoomMinimum = 0.15, WheelZoomMaximum = 3.0;
inline constexpr qreal NodeZoomMargin = 32, FitZoomMargin = 24;
} // namespace an::metrics
