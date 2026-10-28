#include "shared/constants.h"
#include "shared/strings.h"
#include "core/nodedefinition.h"
#include "nodepresentation.h"
#include "window.h"
#include "core/modelcatalog.h"
#include "theme.h"
#include "graphview.h"
#include "projectopener.h"
#include "core/history.h"
#include <algorithm>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSyntaxHighlighter>
#include <QToolButton>
#include <QTextCharFormat>
#include <QUuid>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QStyle>
#include <QScopedValueRollback>
#include <QSystemTrayIcon>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QAbstractTextDocumentLayout>
#include <QKeyEvent>
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QUndoCommand>
#include <QGraphicsProxyWidget>
#include <QClipboard>
#include <QMimeData>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

namespace an {
namespace {
Json historyShape(const Flow &flow, bool positions = true) {
    auto value = flow.toJson(); value.erase(strings::NextNodeId);
    if (!positions) for (auto &node : value[strings::Nodes]) { node.erase(strings::X); node.erase(strings::Y); }
    return value;
}
QString textChangeKey(const Flow &before, const Flow &after) {
    auto a = historyShape(before), b = historyShape(after);
    if (a[strings::Edges] != b[strings::Edges] || a[strings::Name] != b[strings::Name] || a[strings::Nodes].size() != b[strings::Nodes].size()) return {};
    QString result;
    for (size_t i = 0; i < a[strings::Nodes].size(); ++i) {
        const auto &old = a[strings::Nodes][i], &node = b[strings::Nodes][i];
        if (old[strings::Id] != node[strings::Id]) return {};
        for (auto it = old.begin(); it != old.end(); ++it) if (it.value() != node[it.key()]) {
            const auto field = QString::fromStdString(it.key());
            if (!result.isEmpty()) return {};
            QString suffix;
            if (field == strings::Rules || field == strings::UserChoices) {
                std::function<bool(const Json &, const Json &, const QString &)> visit = [&](const Json &from, const Json &to, const QString &path) {
                    if (from == to) return true;
                    if (from.is_string() && to.is_string()) { if (!suffix.isEmpty()) return false; suffix = path; return true; }
                    if (from.type() != to.type() || from.size() != to.size()) return false;
                    if (from.is_array()) { for (size_t i = 0; i < from.size(); ++i) if (!visit(from[i], to[i], path + strings::Colon + QString::number(i))) return false; return true; }
                    if (from.is_object()) { for (auto item = from.begin(); item != from.end(); ++item) if (!to.contains(item.key()) || !visit(item.value(), to[item.key()], path + strings::Colon + QString::fromStdString(item.key()))) return false; return true; }
                    return false;
                };
                if (!visit(it.value(), node[it.key()], {}) || suffix.isEmpty()) return {};
            } else if (!QStringList{strings::Title,strings::Prompt,strings::Schema,strings::ScriptPath,strings::ScriptDirectory,strings::ShellDirectory,strings::ShellCommand,strings::ProjectPath,strings::ScheduleMonth,strings::ScheduleDay,strings::ScheduleHour,strings::ScheduleMinute}.contains(field)) return {};
            result = QString::number(old[strings::Id].get<Id>()) + strings::Colon + field + suffix;
        }
    }
    return result;
}
class FlowEditCommand : public QUndoCommand {
public:
    Flow before, after;
    bool positionOnly;
    FlowEditCommand(Flow old, Flow current, quint64 group, std::function<void(const Flow &)> apply)
        : before(std::move(old)), after(std::move(current)), positionOnly(historyShape(before, false) == historyShape(after, false)), group_(group), apply_(std::move(apply)) {
        mergeKey_ = positionOnly ? strings::Positions : textChangeKey(before, after);
        setText(positionOnly ? strings::MoveNodeCommand : before.nodes.size() < after.nodes.size() ? strings::AddNodeCommand : before.nodes.size() > after.nodes.size() ? strings::DeleteNodeCommand : before.edges != after.edges ? strings::EditConnectionCommand : strings::EditNodeCommand);
    }
    int removedNodes(bool redo) const {
        const auto &from = redo ? before : after, &to = redo ? after : before;
        int count = 0; for (const auto &[id, node] : from.nodes) if (!to.nodes.contains(id)) ++count; return count;
    }
    void undo() override { apply_(before); }
    void redo() override { if (first_) first_ = false; else apply_(after); }
    int id() const override { return mergeKey_.isEmpty() ? -1 : defaults::EditCommandId; }
    bool mergeWith(const QUndoCommand *command) override {
        auto *next = dynamic_cast<const FlowEditCommand *>(command);
        if (!next || next->group_ != group_ || next->mergeKey_ != mergeKey_ || (!positionOnly && next->time_ - time_ > time::EditMergeMilliseconds)) return false;
        after = next->after; time_ = next->time_; return true;
    }
private:
    bool first_ = true;
    QString mergeKey_;
    quint64 group_;
    qint64 time_ = QDateTime::currentMSecsSinceEpoch();
    std::function<void(const Flow &)> apply_;
};
class HistoryDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem styled(option); initStyleOption(&styled, index);
        const auto html = index.data(Qt::UserRole + 1).toString();
        if (html.isEmpty()) { QStyledItemDelegate::paint(painter, option, index); return; }
        const auto *style = styled.widget ? styled.widget->style() : QApplication::style();
        const auto rect = style->subElementRect(QStyle::SE_ItemViewItemText, &styled, styled.widget).adjusted(2, 2, -2, -2);
        styled.text.clear(); style->drawControl(QStyle::CE_ItemViewItem, &styled, painter, styled.widget);
        QTextDocument document; document.setDocumentMargin(0); document.setDefaultFont(styled.font); document.setHtml(html); document.setTextWidth(rect.width());
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = styled.palette;
        context.palette.setColor(QPalette::Text, styled.palette.color(styled.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        painter->save(); painter->setClipRect(rect); painter->translate(rect.topLeft()); document.documentLayout()->draw(painter, context); painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto html = index.data(Qt::UserRole + 1).toString();
        if (html.isEmpty()) return QStyledItemDelegate::sizeHint(option, index);
        const auto *list = qobject_cast<const QListWidget *>(parent());
        const int width = qMax(metrics::ProjectRowMinimumTextWidth, list ? list->viewport()->width() - metrics::ProjectRowPadding : option.rect.width() - metrics::ProjectRowPadding);
        QTextDocument document; document.setDocumentMargin(0); document.setDefaultFont(option.font); document.setHtml(html); document.setTextWidth(width);
        return QSize(width + metrics::ProjectRowPadding, qCeil(document.size().height()) + metrics::ProjectRowPadding);
    }
};
class ProjectStatusLabel : public QLabel {
public:
    using QLabel::QLabel;
protected:
    void paintEvent(QPaintEvent *) override {
        // Keep status colors readable even if a parent is temporarily disabled,
        // rather than letting the native style turn the pixmap into grayscale.
        const auto image = pixmap(); if (image.isNull()) return;
        const auto size = image.deviceIndependentSize(); QPainter painter(this);
        painter.drawPixmap(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), image);
    }
};
class ProjectNameLabel : public QLabel {
public:
    using QLabel::QLabel;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        style()->drawItemText(&painter, contentsRect(), alignment(), palette(), isEnabled(), fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width()), QPalette::WindowText);
    }
};
class Highlighter : public QSyntaxHighlighter {
public:
    explicit Highlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
    void highlightBlock(const QString &line) override {
        const std::pair<QString, QString> rules[] = {
            {strings::JsonStringPattern, colors::SyntaxString},
            {strings::JsonLiteralPattern, colors::SyntaxLiteral},
            {strings::VariableHighlightPattern, colors::SyntaxVariable}
        };
        for (auto &[pattern, color] : rules) {
            QTextCharFormat format; format.setForeground(QColor(color));
            auto it = QRegularExpression(pattern).globalMatch(line);
            while (it.hasNext()) { auto m = it.next(); setFormat(int(m.capturedStart()), int(m.capturedLength()), format); }
        }
    }
};
QPushButton *button(const QString &text, QLayout *layout, std::function<void()> action, bool elide = false) {
    auto *b = elide ? new ElidedButton(text) : new QPushButton(text); layout->addWidget(b); QObject::connect(b, &QPushButton::clicked, b, std::move(action)); return b;
}
QString uniquePath(const QString &dir) { return QDir(dir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + strings::FlowFileExtension); }
QIcon projectStatusIcon(bool inputWaiting) {
    QIcon icon;
    for (int size : metrics::ActivityIconSizes) {
        QPixmap image(size, size); image.fill(colors::Transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing); painter.scale(size / metrics::ActivityDesignExtent, size / metrics::ActivityDesignExtent);
        if (inputWaiting) {
            // Speech bubble + exclamation mark: distinct by shape, not just color.
            const QColor amber(colors::InputActivity); painter.setPen(QPen(amber, metrics::ActivityStroke)); painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(metrics::InputBubbleRect, metrics::BubbleRadius, metrics::BubbleRadius);
            for (const auto &line : metrics::InputBubbleTail) painter.drawLine(line);
            painter.setPen(QPen(amber, metrics::InputMarkStroke, Qt::SolidLine, Qt::RoundCap)); painter.drawLine(metrics::InputMarkLine); painter.drawPoint(metrics::InputMarkDot);
        } else {
            const QColor blue(colors::RunningActivity); painter.setPen(QPen(blue, metrics::ActivityStroke)); painter.setBrush(Qt::NoBrush); painter.drawEllipse(metrics::ActivityCircleRect);
            painter.setPen(Qt::NoPen); painter.setBrush(blue);
            QPolygonF triangle; for (const auto &point : metrics::PlayPoints) triangle << point; painter.drawPolygon(triangle);
        }
        painter.end(); icon.addPixmap(image);
    }
    return icon;
}
QIcon applicationIcon() {
    QIcon icon;
    for (int size : metrics::ApplicationIconSizes) {
        QPixmap image(size, size); image.fill(colors::Transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing); painter.scale(size / metrics::ApplicationDesignExtent, size / metrics::ApplicationDesignExtent);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(colors::MenuBackground)); painter.drawRoundedRect(metrics::ApplicationIconRect, metrics::ApplicationRadius, metrics::ApplicationRadius);
        painter.setPen(QPen(QColor(colors::ApplicationIconStroke), metrics::ApplicationStroke));
        for (const auto &line : metrics::ApplicationEdges) painter.drawLine(line);
        painter.setPen(Qt::NoPen); painter.setBrush(QColor(colors::ApplicationIconFill));
        for (auto point : metrics::ApplicationNodes) painter.drawEllipse(point, metrics::ApplicationNodeRadius, metrics::ApplicationNodeRadius);
        painter.end(); icon.addPixmap(image);
    }
    return icon;
}
std::string str(const QString &s) { return s.toUtf8().toStdString(); }
QString qs(const Json &j) { return QString::fromUtf8(j.get<std::string>()); }
QStringList modelSelectionErrors(const Flow &flow) {
    QStringList errors;
    for (const auto &[id, node] : flow.nodes) if (nodeDefinition(node.kind).requiresModel && node.model.isEmpty())
        errors << strings::MissingModel + (node.title.isEmpty() ? kindName(node.kind) : node.title);
    return errors;
}
}
Window::Window(bool smoke) : smoke_(smoke) {
    setWindowTitle(strings::ApplicationName); resize(metrics::WindowWidth, metrics::WindowHeight);
    dataDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir_ + strings::FlowDirectorySuffix);
    editGraph_ = new Graph; editGraph_->setParent(this);
    workGraph_ = new Graph; workGraph_->setParent(this); workGraph_->readOnly = true;
    coordinator_ = new Coordinator(this);
    runner_ = new Runner(this);
    modelCatalog_ = new ModelCatalog(this);
    if (!smoke_) editGraph_->modelCatalog = workGraph_->modelCatalog = modelCatalog_;
    buildUi();
    qApp->installEventFilter(this);
    editGraph_->newNodePosition = [this] { return nextNodePosition(); };
    static_cast<GraphView *>(editView_)->interactionFinished = [this](std::optional<Id> node, QPointF point) {
        if (current_ < 0 || current_ >= int(documents_.size())) return;
        auto &document = documents_[size_t(current_)];
        if (node && editGraph_->nodeExists(*node)) { document.creationAnchor = editGraph_->flow.nodes.at(*node).position; document.offsetCreation = true; }
        else if (!node) { document.creationAnchor = point; document.offsetCreation = false; }
    };
    static_cast<GraphView *>(editView_)->interactionStarted = [this] { ++historyGroup_; };
    editGraph_->editsLocked = [this] { return currentFlowRunning(); };
    connect(editGraph_, &Graph::editRejected, this, [this] {
        QMessageBox::warning(this, strings::RunningFlowTitle, strings::RunningFlowEditWarning, QMessageBox::Ok);
    });
    static_cast<GraphView *>(editView_)->beforePortEdit = [this] { return editGraph_->canEdit(); };
    static_cast<GraphView *>(workView_)->beforePortEdit = [] { return false; };
    connect(workGraph_, &QtNodes::AbstractGraphModel::nodePositionUpdated, this, [this](Id id) {
        if (project_ >= 0 && project_ < int(projects_.size()) && workGraph_->nodeExists(id)) projects_[size_t(project_)].workPositions[id] = workGraph_->flow.nodes.at(id).position;
    });
    historySaveTimer_ = new QTimer(this); historySaveTimer_->setSingleShot(true); historySaveTimer_->setInterval(time::HistorySaveMilliseconds);
    connect(historySaveTimer_, &QTimer::timeout, this, &Window::flushHistory);
    connect(coordinator_, &Coordinator::executionChanged, this, [this](const QString &id, const NodeExecution &entry) { recordNodeExecution(id, entry); });
    connect(coordinator_, &Coordinator::projectCreated, this, [this](const QString &id, const QString &flow, const QString &directory) {
        Project project{QFileInfo(directory).fileName(), directory, flow, {}}; project.historyId = id;
        projects_.push_back(std::move(project)); syncCoordinatorRegistry(); persist();
        if (current_ >= 0 && documents_[size_t(current_)].path == flow) refreshProjects();
    });
    connect(coordinator_, &Coordinator::sessionStarted, this, [this](const QString &id) {
        for (size_t index = 0; index < projects_.size(); ++index) if (projects_[index].historyId == id) {
            projects_[index].results.clear(); projects_[index].runFlow = coordinator_->activeFlow(id);
            if (int(index) == project_) selectProject(project_);
        }
        refreshProjectStatusIcons(); updateExecutionUi();
    });
    connect(coordinator_, &Coordinator::userInputRequested, this, [this](const QString &projectId, const UserInputRequest &request) {
        inputPrompts_.push_back({projectId, request, {}}); refreshUserInputPanel();
        refreshProjectStatusIcons();
        statusBar()->showMessage(strings::WaitingForUserInput, time::StatusInputMilliseconds);
    });
    connect(coordinator_, &Coordinator::userInputClosed, this, [this](const QString &id) {
        std::erase_if(inputPrompts_, [&](const InputPrompt &prompt) { return prompt.request.id == id; }); refreshUserInputPanel(); refreshProjectStatusIcons();
    });
    connect(coordinator_, &Coordinator::activityChanged, this, [this] {
        updateExecutionUi(); refreshProjectStatusIcons();
        if (!busy()) flushHistory();
    });
    connect(editGraph_, &Graph::edited, this, [this] { changed(); });
    connect(coordinator_, &Coordinator::resultChanged, this, [this](const QString &projectId, Id id, const Result &result) {
        for (size_t index = 0; index < projects_.size(); ++index) if (projects_[index].historyId == projectId) {
            projects_[index].results[id] = result;
            if (int(index) == project_ && workGraph_->nodeExists(id)) {
                workGraph_->states[id] = result.state; emit workGraph_->nodeUpdated(id);
                if (workSelected_ == id) showWorkResult();
            }
            break;
        }
    });
    connect(coordinator_, &Coordinator::sessionFinished, this, [this](const QString &id, bool ok, const QString &message) {
        flushHistory();
        if (project_ >= 0 && projects_[size_t(project_)].historyId == id) {
            status_->setText(message);
            if (!ok && message != strings::ExecutionCancelled && !smoke_ && !quitting_ && !suppressRunWarnings_) QMessageBox::warning(this, strings::ExecutionResultTitle, message);
        }
        statusBar()->showMessage(message, time::StatusLongMilliseconds);
    });
    auto *animation = new QTimer(this); animation->setInterval(time::AnimationMilliseconds);
    connect(animation, &QTimer::timeout, this, [this] {
        ++workGraph_->frame;
        for (auto &[id, state] : workGraph_->states) if (state == State::Running) emit workGraph_->nodeUpdated(id);
    }); animation->start();
    if (!smoke_) restore();
    if (!smoke_) modelCatalog_->setConfig(config_);
    if (documents_.empty()) newFlow(); else { refreshActivities(); selectFlow(0); }
    if (!smoke_) QTimer::singleShot(0, modelCatalog_, [this] { modelCatalog_->refreshAll(); });
    setupTray();
}
Window::~Window() {
    qApp->removeEventFilter(this);
    coordinator_->disconnect(this); runner_->disconnect(this); coordinator_->cancelAll();
    delete coordinator_; // Clear Runner host callbacks before QObject child teardown.
}
bool Window::busy() const { return runner_->running() || coordinator_->running(); }
QPointF Window::nextNodePosition() const {
    const auto offset = 2 * QFontMetrics(editView_->font()).lineSpacing();
    if (current_ >= 0 && current_ < int(documents_.size())) {
        const auto &document = documents_[size_t(current_)];
        if (document.creationAnchor) return *document.creationAnchor + (document.offsetCreation ? QPointF(offset, offset) : QPointF());
    }
    QRectF rightmost;
    bool found = false;
    for (auto *item : editScene_->items()) {
        if (!dynamic_cast<QtNodes::NodeGraphicsObject *>(item) && !dynamic_cast<QtNodes::ConnectionGraphicsObject *>(item)) continue;
        const auto bounds = item->sceneBoundingRect();
        if (!found || bounds.right() > rightmost.right()) { rightmost = bounds; found = true; }
    }
    return found ? QPointF(rightmost.right() + offset, rightmost.top()) : QPointF();
}
void Window::updateInputShortcutHint() {
    if (inputShortcutHint_) inputShortcutHint_->setVisible(controlPressed_ && !activeInput_.isEmpty() && inputText_->isEnabled());
}
bool Window::eventFilter(QObject *object, QEvent *event) {
    if (object == this && event->type() == QEvent::WindowDeactivate) { controlPressed_ = false; updateInputShortcutHint(); }
    auto *widget = qobject_cast<QWidget *>(object);
    if (widget && editorTarget(widget)) {
        if (event->type() == QEvent::FocusIn) ++historyGroup_;
        if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)) {
                if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) stepHistory(key->matches(QKeySequence::Redo));
                event->accept(); return true;
            }
        }
    }
    if (widget && (widget == this || isAncestorOf(widget)) && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        controlPressed_ = key->key() == Qt::Key_Control ? event->type() == QEvent::KeyPress : key->modifiers().testFlag(Qt::ControlModifier);
        updateInputShortcutHint();
        if (object == inputText_ && inputText_->hasFocus() && inputText_->isVisible() && inputText_->isEnabled() &&
            (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers() == Qt::ControlModifier) {
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat())
                if (auto *first = qobject_cast<QPushButton *>(inputButtons_->itemAt(0) ? inputButtons_->itemAt(0)->widget() : nullptr)) first->click();
            return true;
        }
    }
    return QMainWindow::eventFilter(object, event);
}
bool Window::currentFlowRunning() const {
    if (current_ < 0 || current_ >= int(documents_.size())) return false;
    const auto path = documents_[size_t(current_)].path;
    for (const auto &project : projects_) if (project.flowPath == path && coordinator_->running(project.historyId)) return true;
    return false;
}
void Window::updateExecutionUi() {
    activities_->setEnabled(true); projectList_->setEnabled(true); tabs_->setTabEnabled(0, true);
    const bool selected = project_ >= 0 && project_ < int(projects_.size());
    const bool running = selected && coordinator_->running(projects_[size_t(project_)].historyId);
    runButton_->setEnabled(selected && !running);
    stopButton_->setEnabled(running); stopButton_->setToolTip(strings::StopProjectTooltip);
    if (currentFlowRunning()) { textPane_->hide(); field_.clear(); }
}
void Window::updateTerminalButton() {
    if(auto *button=findChild<QPushButton *>(strings::AddNodeEnd)) button->setEnabled(std::none_of(editGraph_->flow.nodes.begin(),editGraph_->flow.nodes.end(),[](const auto &item){return nodeDefinition(item.second.kind).terminal;}));
}
void Window::syncCoordinatorRegistry() {
    coordinator_->flows.clear(); coordinator_->projects.clear(); coordinator_->config = config_;
    for (const auto &doc : documents_) coordinator_->flows[doc.path] = doc.flow;
    for (const auto &project : projects_) coordinator_->projects.push_back({project.historyId, project.flowPath, project.directory});
}
bool Window::confirmStopProjects(const QStringList &ids) {
    QStringList active;
    for (const auto &id : ids) if (coordinator_->running(id)) active.push_back(id);
    if (active.isEmpty()) return true;
    if (QMessageBox::warning(this, strings::RunningProjectTitle, strings::StopProjectBeforeOperation, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;
    const QScopedValueRollback<bool> suppress(suppressRunWarnings_, true);
    for (const auto &id : active) coordinator_->cancelProject(id);
    return true;
}
void Window::setupTray() {
    setWindowIcon(applicationIcon());
    trayIcon_ = new QSystemTrayIcon(windowIcon(), this); trayIcon_->setObjectName(strings::SystemTrayIcon); trayIcon_->setToolTip(strings::ApplicationName);
    auto *menu = new QMenu(this); menu->setObjectName(strings::TrayMenu);
    auto *show = menu->addAction(strings::Show, this, &Window::showWindow); show->setObjectName(strings::TrayShow);
    menu->addSeparator();
    auto *quit = menu->addAction(strings::Quit, this, &Window::requestQuit); quit->setObjectName(strings::TrayQuit);
    trayIcon_->setContextMenu(menu);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showWindow();
    });
    if (!smoke_) trayIcon_->show();
}
void Window::showWindow() {
    if (isMinimized()) setWindowState(windowState() & ~Qt::WindowMinimized);
    show(); raise(); activateWindow();
}
void Window::requestQuit() {
    if (quitting_) return; // A second request during a confirmation dialog is ignored.
    quitting_ = true;
    if (!close()) { quitting_ = false; return; }
    trayIcon_->hide(); emit quitRequested();
}
void Window::refreshFlowChoices() {
    std::vector<std::pair<QString, QString>> choices;
    for (const auto &doc : documents_) choices.push_back({doc.path, doc.flow.name});
    editGraph_->setFlowChoices(choices); workGraph_->setFlowChoices(choices);
}
void Window::buildUi() {
    auto *file = menuBar()->addMenu(strings::FileMenuLabel);
    file->setObjectName(strings::FileMenu);
    file->addAction(strings::NewFlowName, QKeySequence::New, this, [this] { newFlow(); });
    file->addAction(strings::OpenAction, QKeySequence::Open, this, [this] { openFlow(); });
    file->addAction(strings::SaveAction, QKeySequence::Save, this, [this] { saveFlow(); });
    file->addAction(strings::SaveAsAction, QKeySequence::SaveAs, this, [this] { saveFlow(true); });
    file->addSeparator(); file->addAction(strings::Quit, QKeySequence::Quit, this, &Window::requestQuit);
    auto *edit = menuBar()->addMenu(strings::EditMenuLabel);
    undoAction_ = edit->addAction(strings::UndoAction, this, [this] { stepHistory(false); }); undoAction_->setShortcuts(QKeySequence::Undo); undoAction_->setObjectName(strings::UndoFlowEdit);
    redoAction_ = edit->addAction(strings::RedoAction, this, [this] { stepHistory(true); }); redoAction_->setShortcuts(QKeySequence::Redo); redoAction_->setObjectName(strings::RedoFlowEdit);
    auto *tools = menuBar()->addMenu(strings::ToolLabel);
    tools->addAction(strings::ValidateAction, QKeySequence(strings::F7), this, [this] { validateFlow(); });
    tools->addAction(strings::CliSettingsAction, this, [this] { settings(); });
    tools->addAction(strings::RefreshModelsAction, this, [this] { if (!smoke_) modelCatalog_->refreshAll(true); });
    auto *help = menuBar()->addMenu(strings::HelpMenuLabel);
    help->addAction(strings::UsageAction, this, [this] {
        QMessageBox::information(this, strings::UsageAction, strings::UsageHelp);
    });
    auto *root = new QWidget; auto *row = new QHBoxLayout(root); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0);
    auto *bar = new QWidget; bar->setFixedWidth(metrics::FlowRailWidth); auto *barLayout = new QVBoxLayout(bar); barLayout->setContentsMargins(metrics::SpaceTiny, metrics::SpaceNormal, metrics::SpaceTiny, metrics::SpaceNormal);
    auto *flowScroll = new QScrollArea; flowScroll->setObjectName(strings::FlowScroll); flowScroll->setWidgetResizable(true);
    flowScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    activities_ = new QWidget; activities_->setObjectName(strings::FlowRail); activities_->setMinimumWidth(metrics::FlowRailMinimumWidth);
    activityLayout_ = new QVBoxLayout(activities_); activityLayout_->setContentsMargins(metrics::SpaceNormal, metrics::SpaceComfortable, metrics::SpaceNormal, metrics::SpaceComfortable); activityLayout_->setSpacing(metrics::SpaceLarge);
    flowScroll->setWidget(activities_); barLayout->addWidget(flowScroll, 1); row->addWidget(bar);
    tabs_ = new QTabWidget; tabs_->setObjectName(strings::MainTabs); row->addWidget(tabs_, 1);
    editorSplit_ = new QSplitter;
    auto *left = new QWidget; auto *leftLayout = new QVBoxLayout(left);
    leftLayout->addWidget(new QLabel(strings::AddNodeCommand));
    for (const auto &definition : nodeDefinitions()) {
        if (definition.source) continue;
        const auto k = definition.kind;
        auto *add = button(strings::AddNodePrefix + kindName(k), leftLayout, [this, k] {
            auto id = editGraph_->addNode(kindKey(k));
            if (id == QtNodes::InvalidNodeId) statusBar()->showMessage(strings::CannotAddNode, time::StatusShortMilliseconds);
            else selectNode(id);
        }, true);
        add->setToolTip(strings::AddNodePrefix + kindName(k) + strings::Newline + nodeDescription(k)); add->setObjectName(strings::AddNode + kindKey(k));
        if(hasSpecialNodeBackground(k)) add->setStyleSheet(QString(strings::NodePaletteButtonStyle).arg(nodeBackground(k).name(),nodeForeground(k).name(),nodeAccent(k).name(),nodeBackground(k).lighter(metrics::NodeButtonHoverLightnessPercent).name()));
    }
    auto *validate = button(strings::ValidateAction, leftLayout, [this] { validateFlow(); }, true); validate->setToolTip(strings::ValidateAction);
    leftLayout->addStretch(); left->setObjectName(strings::NodePalette);
    left->setMinimumWidth(metrics::PaletteMinimumWidth); editorSplit_->addWidget(left);
    editScene_ = new QtNodes::BasicGraphicsScene(*editGraph_, this); editView_ = new GraphView(editScene_);
    editView_->setScaleRange(metrics::WheelZoomMinimum, metrics::WheelZoomMaximum); editorSplit_->addWidget(editView_);
    editView_->setObjectName(strings::FlowEditorView);
    // QtNodes' commands drive mouse gestures; our per-document snapshots own
    // user-facing undo so parameter edits and structural edits share one order.
    for (auto *action : editView_->actions()) {
        if (action->shortcuts().contains(QKeySequence(QKeySequence::Undo)) || action->shortcuts().contains(QKeySequence(QKeySequence::Redo))) { editView_->removeAction(action); action->deleteLater(); }
        else if (action == editView_->deleteSelectionAction()) {
            QObject::disconnect(action, nullptr, editView_, nullptr); connect(action, &QAction::triggered, this, [this] { deleteEditorObjects(); });
        } else if (action->shortcut() == QKeySequence(QKeySequence::Paste) || action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_D)) {
            const bool paste = action->shortcut() == QKeySequence(QKeySequence::Paste);
            QObject::disconnect(action, nullptr, editView_, nullptr);
            connect(action, &QAction::triggered, this, [this, paste] { if (!editGraph_->canEdit() || !canPasteEditorObjects(paste)) return; editorOperation([this, paste] { if (paste) editView_->onPasteObjects(); else editView_->onDuplicateSelectedObjects(); }); });
        }
    }
    connect(&editScene_->undoStack(), &QUndoStack::indexChanged, this, [this] {
        QTimer::singleShot(0, this, [this] { if (editScene_->undoStack().count()) editScene_->undoStack().clear(); });
    });
    editView_->setToolTip(strings::ZoomToFitTooltip);
    connect(editScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectNode(id); });
    connect(editScene_, &QtNodes::BasicGraphicsScene::nodeContextMenu, this, [this](Id id, const QPointF &point) {
        if (!editGraph_->nodeExists(id)) return;
        QMenu menu(this); menu.setObjectName(strings::NodeContextMenu);
        auto *remove = menu.addAction(strings::FileDeletedLabel);
        const auto kind = editGraph_->flow.nodes.at(id).kind;
        remove->setEnabled(!nodeDefinition(kind).source);
        connect(remove, &QAction::triggered, this, [this, id] { deleteEditorObjects(id); });
        menu.exec(editView_->viewport()->mapToGlobal(editView_->mapFromScene(point)));
    });
    connect(editGraph_, &QtNodes::AbstractGraphModel::nodeDeleted, this, [this](Id id) { if (selected_ == id) { selected_ = 0; textPane_->hide(); selectNode(0); } });
    connect(editGraph_, &QtNodes::AbstractGraphModel::nodeCreated, this, [this](Id id) {
        if (current_ >= 0 && current_ < int(documents_.size())) {
            auto &document = documents_[size_t(current_)]; document.creationAnchor = editGraph_->flow.nodes.at(id).position; document.offsetCreation = true;
        }
        const int document = current_;
        QTimer::singleShot(0, this, [this, id, document] {
            if (current_ != document || !editGraph_->nodeExists(id) || !editView_->isVisible()) return;
            if (auto *item = editScene_->nodeGraphicsObject(id))
                if (!editView_->mapToScene(editView_->viewport()->rect()).boundingRect().contains(item->sceneBoundingRect())) static_cast<GraphView *>(editView_)->zoomToFit();
        });
    });
    connect(editGraph_, &Graph::textEditRequested, this, [this](Id id, const QString &field) { selectNode(id); showText(field); });
    connect(editGraph_, &Graph::nodeActivated, this, [this](Id id) { selectNode(id); });
    textPane_ = new QWidget; textPane_->setObjectName(strings::TextEditorPane); auto *textLayout = new QVBoxLayout(textPane_);
    auto *heading = new QHBoxLayout; textTitle_ = new QLabel; heading->addWidget(textTitle_, 1);
    button(strings::CloseAction, heading, [this] { textPane_->hide(); field_.clear(); }); textLayout->addLayout(heading);
    text_ = new QPlainTextEdit; text_->setObjectName(strings::ParameterEditor); text_->setFont(QFont(strings::CodeFont, metrics::EditorCodeFontPoints)); new Highlighter(text_->document()); textLayout->addWidget(text_, 1);
    textError_ = new QLabel; textError_->setWordWrap(true); textError_->setStyleSheet(strings::ValidationErrorStyle); textLayout->addWidget(textError_);
    connect(text_, &QPlainTextEdit::textChanged, this, [this] {
        if (loading_ || !editGraph_->nodeExists(selected_) || field_.isEmpty()) return;
        if (!editGraph_->canEdit()) return;
        auto n = editGraph_->flow.nodes.at(selected_);
        if (field_ == strings::Schema) n.schema = text_->toPlainText(); else if (field_ == strings::ShellCommand) n.shellCommand = text_->toPlainText(); else n.prompt = text_->toPlainText();
        editGraph_->updateNode(n); textError_->clear();
        if (field_ == strings::Schema && !n.schema.trimmed().isEmpty()) {
            try { checkSchema(parseJson(n.schema)); } catch (const std::exception &e) { textError_->setText(QString::fromUtf8(e.what())); }
        }
    });
    editorSplit_->addWidget(textPane_); textPane_->hide(); editorSplit_->setSizes({metrics::EditorPaletteWidth, metrics::EditorCanvasWidth, metrics::EditorTextWidth}); editorSplit_->setStretchFactor(1, 1);
    tabs_->addTab(editorSplit_, strings::FlowEditorTab);
    auto *workSplit = new QSplitter;
    auto *projectPane = new QWidget; auto *pl = new QVBoxLayout(projectPane); pl->addWidget(new QLabel(strings::ProjectLabel));
    projectList_ = new QListWidget; pl->addWidget(projectList_, 1);
    connect(projectList_, &QListWidget::currentRowChanged, this, [this](int rowIndex) {
        if (rowIndex >= 0 && projectList_->item(rowIndex)) selectProject(projectList_->item(rowIndex)->data(Qt::UserRole).toInt());
    });
    button(strings::AddProjectDirectoryAction, pl, [this] { addProject(); });
    workSplit->addWidget(projectPane);
    auto *center = new QWidget; auto *cl = new QVBoxLayout(center);
    auto *toolbar = new QHBoxLayout; status_ = new QLabel(strings::SelectProjectHint); status_->setWordWrap(true); toolbar->addWidget(status_, 1);
    runButton_ = button(strings::RunAction, toolbar, [this] { executeFlow(); });
    stopButton_ = button(strings::StopAction, toolbar, [this] {
        if (project_ < 0 || project_ >= int(projects_.size())) return;
        const auto id = projects_[size_t(project_)].historyId;
        if (!coordinator_->running(id)) return;
        if (QMessageBox::warning(this, strings::StopFlowTitle, strings::StopFlowConfirmation, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok) coordinator_->cancelProject(id);
    }); stopButton_->setEnabled(false); cl->addLayout(toolbar);
    workInputSplit_ = new QSplitter(Qt::Vertical); workInputSplit_->setObjectName(strings::WorkInputSplitter); workInputSplit_->setChildrenCollapsible(false); workInputSplit_->setHandleWidth(metrics::SplitterHandleWidth); cl->addWidget(workInputSplit_, 1);
    workScene_ = new QtNodes::BasicGraphicsScene(*workGraph_, this); workView_ = new GraphView(workScene_); workView_->setMinimumHeight(metrics::WorkGraphMinimumHeight); workView_->setScaleRange(metrics::WheelZoomMinimum, metrics::WheelZoomMaximum); workInputSplit_->addWidget(workView_);
    workView_->setToolTip(strings::ZoomToFitTooltip);
    inputPanel_ = new QWidget; inputPanel_->setObjectName(strings::UserInputPanel); auto *inputLayout = new QVBoxLayout(inputPanel_); inputLayout->setContentsMargins(0, 0, 0, 0); inputLayout->setSpacing(metrics::SpaceSmall);
    inputSelector_ = new QComboBox; inputSelector_->setObjectName(strings::UserInputSelector); styleComboBoxPopup(inputSelector_); inputLayout->addWidget(inputSelector_);
    inputMessage_ = new ContentView; inputMessage_->setObjectName(strings::UserInputMessage); inputMessage_->setCompactMessage(metrics::UserMessageLines); inputLayout->addWidget(inputMessage_);
    inputText_ = new QPlainTextEdit; inputText_->setObjectName(strings::UserInputText); inputText_->setPlaceholderText(strings::UserMessagePlaceholder); inputText_->setMinimumHeight(metrics::UserInputMinimumHeight); inputLayout->addWidget(inputText_, 1);
    inputText_->setToolTip(strings::InputShortcutTooltip);
    auto *choices = new QWidget; inputButtons_ = new QHBoxLayout(choices); inputButtons_->setContentsMargins(0, 0, 0, 0);
    auto *choiceScroll = new QScrollArea; choiceScroll->setWidgetResizable(true); choiceScroll->setWidget(choices); choiceScroll->setFrameShape(QFrame::NoFrame); choiceScroll->setMinimumHeight(metrics::ChoiceScrollMinimumHeight); choiceScroll->setMaximumHeight(metrics::ChoiceScrollMaximumHeight); inputLayout->addWidget(choiceScroll);
    workInputSplit_->addWidget(inputPanel_); workInputSplit_->setStretchFactor(0, 1); workInputSplit_->setStretchFactor(1, 0); inputPanel_->hide();
    workInputSplit_->handle(1)->setToolTip(strings::ResizeInputPaneTooltip);
    connect(workInputSplit_, &QSplitter::splitterMoved, this, [this] {
        const auto sizes = workInputSplit_->sizes(); if (!inputPanel_->isHidden() && sizes.size() == 2 && sizes[1] > 0) inputPaneHeight_ = sizes[1];
    });
    connect(inputSelector_, &QComboBox::currentIndexChanged, this, [this] {
        activeInput_ = inputSelector_->currentData().toString(); refreshUserInputPanel(); focusUserInputNode();
    });
    connect(inputSelector_, &QComboBox::activated, this, [this] { focusUserInputNode(); });
    connect(inputText_, &QPlainTextEdit::textChanged, this, [this] {
        for (auto &prompt : inputPrompts_) if (prompt.request.id == activeInput_) { prompt.draft = inputText_->toPlainText(); break; }
    });
    connect(workScene_, &QtNodes::BasicGraphicsScene::nodeClicked, this, [this](Id id) { selectWorkNode(id); });
    connect(workGraph_, &Graph::nodeActivated, this, [this](Id id) { selectWorkNode(id); });
    connect(workGraph_, &Graph::textEditRequested, this, [this](Id id, const QString &) { selectWorkNode(id); });
    auto tooltips = [this](Graph *graph, QtNodes::BasicGraphicsScene *scene) {
        auto update = [graph, scene](Id id) {
            if (auto *item = scene->nodeGraphicsObject(id)) {
                item->setToolTip(nodeDescription(graph->flow.nodes.at(id).kind));
                if (graph->readOnly) item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            }
        };
        connect(graph, &QtNodes::AbstractGraphModel::nodeCreated, this, update);
        connect(graph, &QtNodes::AbstractGraphModel::modelReset, this, [graph, update] { for (auto &[id, node] : graph->flow.nodes) update(id); });
    };
    tooltips(editGraph_, editScene_); tooltips(workGraph_, workScene_);
    workSplit->addWidget(center);
    auto *details = new QWidget; auto *dl = new QVBoxLayout(details);
    nodeStatus_ = new QLabel(strings::SelectNodeHint); nodeStatus_->setWordWrap(true); dl->addWidget(nodeStatus_);
    auto *detailSplit = new QSplitter(Qt::Vertical); detailSplit->setObjectName(strings::WorkDetailsSplitter); dl->addWidget(detailSplit, 1);
    auto *historyPane = new QWidget; auto *hl = new QVBoxLayout(historyPane); hl->setContentsMargins(0, 0, 0, 0);
    auto *historyHeading = new QHBoxLayout; historyHeading->addWidget(new QLabel(strings::ExecutionHistoryTitle), 1);
    followHistoryButton_ = new QToolButton; followHistoryButton_->setObjectName(strings::FollowLatestExecution);
    followHistoryButton_->setText(strings::FollowLatestAction); followHistoryButton_->setCheckable(true); followHistoryButton_->setChecked(true);
    followHistoryButton_->setToolTip(strings::FollowLatestTooltip);
    historyHeading->addWidget(followHistoryButton_); hl->addLayout(historyHeading);
    connect(followHistoryButton_, &QToolButton::toggled, this, [this](bool follow) { followLatestHistory_ = follow; if (follow) refreshWorkHistory(); });
    historyList_ = new QListWidget; historyList_->setObjectName(strings::NodeExecutionHistory); historyList_->setWordWrap(true);
    historyList_->setItemDelegate(new HistoryDelegate(historyList_));
    historyList_->setToolTip(strings::ExecutionHistoryTooltip); hl->addWidget(historyList_, 1); detailSplit->addWidget(historyPane);
    connect(historyList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        selectedHistory_ = item ? item->data(Qt::UserRole).toString() : QString();
        if (item) { followLatestHistory_ = false; followHistoryButton_->setChecked(false); }
        showWorkResult();
    });
    connect(historyList_, &QListWidget::itemClicked, this, [this](QListWidgetItem *) { followLatestHistory_ = false; followHistoryButton_->setChecked(false); });
    auto *logPane = new QWidget; auto *ll = new QVBoxLayout(logPane); ll->setContentsMargins(0, metrics::SpaceMedium, 0, 0);
    ll->addWidget(new QLabel(strings::ProgressLogTitle)); log_ = new ProgressView; ll->addWidget(log_, 1); detailSplit->addWidget(logPane);
    auto *outputPane = new QWidget; auto *ol = new QVBoxLayout(outputPane); ol->setContentsMargins(0, metrics::SpaceMedium, 0, 0);
    ol->addWidget(new QLabel(strings::NextOutputTitle)); output_ = new ContentView;
    auto *outputScroll = new QScrollArea; outputScroll->setWidgetResizable(true); outputScroll->setWidget(output_); ol->addWidget(outputScroll, 1); detailSplit->addWidget(outputPane);
    detailSplit->setSizes({metrics::HistoryPaneHeight, metrics::ProgressPaneHeight, metrics::OutputPaneHeight});
    workSplit->addWidget(details); workSplit->setSizes({metrics::ProjectPaneWidth, metrics::WorkCanvasWidth, metrics::DetailsPaneWidth}); workSplit->setStretchFactor(1, 1);
    tabs_->addTab(workSplit, strings::ProjectWorkTab); setCentralWidget(root);
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        updateUndoActions();
        if (index != 1) return;
        if (project_ >= 0) selectProject(project_);
        else { workGraph_->replace(editGraph_->flow); selectWorkNode(workGraph_->nodeExists(workSelected_) ? workSelected_ : 0); }
        focusUserInputNode();
    });
    setStyleSheet(strings::ApplicationStyle);
    statusBar()->showMessage(strings::CreateOrOpenFlowHint);
}
void Window::changed() {
    if (loading_ || current_ < 0) return;
    auto &doc = documents_[size_t(current_)]; const auto before = doc.flow; doc.flow = editGraph_->flow; doc.dirty = true;
    if (!applyingHistory_ && !editBatch_) recordEdit(before, doc.flow);
    updateTerminalButton();
    for (auto &p : projects_) if (p.flowPath == doc.path && !coordinator_->running(p.historyId)) { p.results.clear(); p.runFlow.reset(); }
    syncCoordinatorRegistry();
    setWindowTitle(strings::WindowTitlePrefix + doc.flow.name + strings::DirtyMarker);
}
bool Window::editorTarget(QWidget *widget) const {
    if (!editorSplit_ || !tabs_ || tabs_->currentIndex() != 0) return false;
    if (widget == editorSplit_ || editorSplit_->isAncestorOf(widget)) return true;
    for (auto *parent = widget; parent; parent = parent->parentWidget())
        if (auto *proxy = parent->graphicsProxyWidget()) return proxy->scene() == editScene_;
    return false;
}
void Window::recordEdit(const Flow &before, const Flow &after) {
    if (current_ < 0 || historyShape(before) == historyShape(after)) return;
    documents_[size_t(current_)].undo->push(new FlowEditCommand(before, after, historyGroup_, [this](const Flow &flow) { applyHistory(flow); }));
    updateUndoActions();
}
void Window::updateUndoActions() {
    if (!undoAction_ || !redoAction_) return;
    const auto stack = current_ >= 0 && current_ < int(documents_.size()) ? documents_[size_t(current_)].undo : nullptr;
    const bool editor = tabs_ && tabs_->currentIndex() == 0;
    undoAction_->setEnabled(editor && stack && stack->canUndo()); redoAction_->setEnabled(editor && stack && stack->canRedo());
    undoAction_->setText(stack && stack->canUndo() ? strings::UndoPrefix + stack->undoText() : strings::UndoAction);
    redoAction_->setText(stack && stack->canRedo() ? strings::RedoPrefix + stack->redoText() : strings::RedoAction);
}
bool Window::confirmNodeDeletion(int count) {
    return count == 0 || QMessageBox::warning(this, strings::DeleteNodeCommand, QString(strings::DeleteNodesConfirmation).arg(count), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok;
}
void Window::stepHistory(bool redo) {
    if (current_ < 0 || tabs_->currentIndex() != 0) return;
    const auto stack = documents_[size_t(current_)].undo;
    const int index = stack->index();
    if (redo ? !stack->canRedo() : !stack->canUndo()) return;
    const auto *command = dynamic_cast<const FlowEditCommand *>(stack->command(redo ? index : index - 1));
    if (!command || (!command->positionOnly && !editGraph_->canEdit())) return;
    if (!confirmNodeDeletion(command->removedNodes(redo))) return;
    if (current_ < 0 || documents_[size_t(current_)].undo != stack || stack->index() != index) return;
    if (!command->positionOnly && !editGraph_->canEdit()) return;
    ++historyGroup_;
    if (redo) stack->redo(); else stack->undo();
    auto &document = documents_[size_t(current_)]; document.dirty = !stack->isClean() || (document.savedNextNodeId && editGraph_->flow.nextNodeId > document.savedNextNodeId);
    setWindowTitle(strings::WindowTitlePrefix + document.flow.name + (document.dirty ? strings::DirtyMarker : strings::Empty)); updateUndoActions();
}
void Window::applyHistory(const Flow &flow) {
    const QScopedValueRollback<bool> applying(applyingHistory_, true);
    editGraph_->restore(flow);
    if (!field_.isEmpty() && editGraph_->nodeExists(selected_)) {
        const auto &node = editGraph_->flow.nodes.at(selected_);
        const auto value = field_ == strings::Schema ? node.schema : field_ == strings::ShellCommand ? node.shellCommand : node.prompt;
        if (text_->toPlainText() != value) {
            const int position = text_->textCursor().position(); const QSignalBlocker block(text_); text_->setPlainText(value);
            auto cursor = text_->textCursor(); cursor.setPosition(qMin(position, int(value.size()))); text_->setTextCursor(cursor);
        }
        textError_->clear();
        if (field_ == strings::Schema && !node.schema.trimmed().isEmpty()) {
            try { checkSchema(parseJson(node.schema)); } catch (const std::exception &error) { textError_->setText(QString::fromUtf8(error.what())); }
        }
    }
}
void Window::editorOperation(const std::function<void()> &operation) {
    const auto before = editGraph_->flow; ++historyGroup_;
    { const QScopedValueRollback<int> batch(editBatch_, editBatch_ + 1); operation(); }
    recordEdit(before, editGraph_->flow);
}
bool Window::canPasteEditorObjects(bool fromClipboard) {
    int ends=int(std::count_if(editGraph_->flow.nodes.begin(),editGraph_->flow.nodes.end(),[](const auto &item){return nodeDefinition(item.second.kind).terminal;}));
    try {
        auto accept=[&](Kind kind){if(nodeDefinition(kind).source)throw std::runtime_error(strings::CannotDuplicateStart);if(nodeDefinition(kind).terminal && ++ends>1)throw std::runtime_error(strings::TooManyEndNodes);};
        if(fromClipboard) {
            const auto *mime=QApplication::clipboard()->mimeData(); if(!mime || !mime->hasFormat(strings::ApplicationQtNodesGraph))return false;
            const auto bytes=mime->data(strings::ApplicationQtNodesGraph); if(bytes.size()>limits::ProcessOutputBytes)return false;
            const auto data=Json::parse(bytes.toStdString());
            for(const auto &item:data.at(strings::Nodes)) {const auto flow=Flow::fromJson(item.at(strings::Payload));if(flow.nodes.size()!=1)return false;accept(flow.nodes.begin()->second.kind);}
        } else for(auto *item:editScene_->selectedItems())if(auto *node=dynamic_cast<QtNodes::NodeGraphicsObject *>(item))accept(editGraph_->flow.nodes.at(node->nodeId()).kind);
        return true;
    } catch(const std::exception &error) {statusBar()->showMessage(QString::fromUtf8(error.what()),time::StatusShortMilliseconds);return false;}
}
void Window::deleteEditorObjects(std::optional<Id> node) {
    if (!editGraph_->canEdit()) return;
    std::vector<Id> nodes; std::vector<QtNodes::ConnectionId> connections;
    auto addNode = [this, &nodes](Id id) {
        if (editGraph_->nodeExists(id) && !nodeDefinition(editGraph_->flow.nodes.at(id).kind).source) nodes.push_back(id);
    };
    if (node) addNode(*node);
    else for (auto *item : editScene_->selectedItems()) {
        if (auto *object = dynamic_cast<QtNodes::NodeGraphicsObject *>(item)) addNode(object->nodeId());
        else if (auto *wire = dynamic_cast<QtNodes::ConnectionGraphicsObject *>(item)) connections.push_back(wire->connectionId());
    }
    if (nodes.empty() && connections.empty()) return;
    if (!editGraph_->canEdit()) return;
    editorOperation([&] { for (auto connection : connections) editGraph_->deleteConnection(connection); for (auto id : nodes) editGraph_->deleteNode(id); });
}
void Window::refreshActivities() {
    syncCoordinatorRegistry();
    refreshFlowChoices();
    while (auto *item = activityLayout_->takeAt(0)) {
        if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); }
        delete item;
    }
    auto tile = [this](const QString &text, const QString &name, std::function<void()> action) {
        auto *button = new QToolButton; button->setProperty(strings::FlowTile, true); button->setFixedSize(metrics::FlowTileExtent, metrics::FlowTileExtent);
        button->setText(text); button->setAccessibleName(name); button->setCursor(Qt::PointingHandCursor);
        activityLayout_->addWidget(button, 0, Qt::AlignHCenter); connect(button, &QToolButton::clicked, this, std::move(action));
        return button;
    };
    for (int i = 0; i < int(documents_.size()); ++i) {
        const auto &doc = documents_[size_t(i)];
        auto *button = tile(doc.flow.name.left(2), doc.flow.name, [this, i] { selectFlow(i); });
        button->setObjectName(strings::FlowTile); button->setCheckable(true); button->setChecked(i == current_);
        button->setToolTip(doc.flow.name + strings::Newline + doc.path);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this, [this, button, i](const QPoint &point) {
            QMenu menu(this);
            menu.addAction(strings::RenameFlowAction, this, [this, i] {
                selectFlow(i); if (!editGraph_->canEdit()) return; bool ok = false;
                auto name = QInputDialog::getText(this, strings::FlowNameLabel, strings::NameLabel, QLineEdit::Normal, editGraph_->flow.name, &ok);
                if (ok && !name.trimmed().isEmpty() && editGraph_->canEdit()) { editGraph_->flow.name = name.trimmed(); changed(); refreshActivities(); }
            });
            menu.addAction(strings::RemoveFromListAction, this, [this, i] { removeFlow(i); });
            menu.exec(button->mapToGlobal(point));
        });
    }
    auto *add = tile(strings::AddSymbol, strings::AddFlowAction, [] {}); add->setObjectName(strings::AddFlowTile); add->setToolTip(strings::AddFlowAction);
    auto *menu = new QMenu(add);
    menu->addAction(strings::NewFlowAction, this, [this] { newFlow(); });
    menu->addAction(strings::AddExistingFlowAction, this, [this] { openFlow(); });
    add->setMenu(menu); add->setPopupMode(QToolButton::InstantPopup);
    activityLayout_->addStretch();
}
void Window::selectFlow(int index) {
    if (index < 0 || index >= int(documents_.size())) return;
    current_ = index; loading_ = true;
    ++historyGroup_; editScene_->undoStack().clear();
    if (!documents_[size_t(index)].undo->count() && documents_[size_t(index)].dirty) documents_[size_t(index)].undo->resetClean();
    editGraph_->replace(documents_[size_t(index)].flow); selected_ = 0; field_.clear(); textPane_->hide();
    if (!documents_[size_t(index)].dirty && !documents_[size_t(index)].savedNextNodeId) documents_[size_t(index)].savedNextNodeId = editGraph_->flow.nextNodeId;
    loading_ = false; selectNode(0); editView_->centerScene();
    setWindowTitle(strings::WindowTitlePrefix + editGraph_->flow.name + (documents_[size_t(index)].dirty ? strings::DirtyMarker : strings::Empty));
    refreshActivities(); refreshProjects();
    updateUndoActions();
    updateTerminalButton();
}
void Window::selectNode(Id id) {
    selected_ = id; field_.clear(); textPane_->hide();
}
void Window::showText(const QString &field) {
    if (!editGraph_->nodeExists(selected_)) return;
    if (!editGraph_->canEdit()) return;
    const auto &n = editGraph_->flow.nodes.at(selected_);
    if (!nodePresentation(n.kind).textFields.contains(field)) return;
    field_ = field; loading_ = true; textError_->clear();
    textTitle_->setText(field == strings::Schema ? strings::SchemaEditorTitle : field == strings::ShellCommand ? strings::ShellEditorTitle : strings::InputPromptLabel);
    text_->setPlainText(field == strings::Schema ? n.schema : field == strings::ShellCommand ? n.shellCommand : n.prompt);
    loading_ = false; textPane_->show(); text_->setFocus();
    const auto id = selected_;
    QTimer::singleShot(0, this, [this, id] {
        if (selected_ == id && textPane_->isVisible())
            if (auto *item = editScene_->nodeGraphicsObject(id)) editView_->ensureVisible(item, metrics::EnsureVisibleMargin, metrics::EnsureVisibleMargin);
    });
}
void Window::newFlow() {
    Document doc; doc.flow = Flow::initial(false); doc.flow.name = QString(strings::IndexedFlowName).arg(documents_.size() + 1); doc.path = uniquePath(dataDir_ + strings::FlowDirectorySuffix); doc.dirty = true;
    documents_.push_back(doc); selectFlow(int(documents_.size()) - 1);
}
void Window::removeFlow(int index) {
    if (index < 0 || index >= int(documents_.size())) return;
    auto &doc = documents_[size_t(index)];
    const auto internalDir = QFileInfo(dataDir_ + strings::FlowDirectorySuffix).canonicalFilePath();
    const bool internal = QFileInfo(doc.path).absoluteDir().canonicalPath().compare(internalDir, Qt::CaseInsensitive) == 0;
    const auto warning = internal
        ? strings::RemoveInternalFlowConfirmation
        : strings::RemoveExternalFlowConfirmation;
    if (QMessageBox::warning(this, strings::RemoveFlowTitle, warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QStringList ids; for (const auto &project : projects_) if (project.flowPath == doc.path) ids.push_back(project.historyId);
    if (!confirmStopProjects(ids)) return;
    if (internal && QFileInfo::exists(doc.path) && !QFile::remove(doc.path)) { reportError(strings::RemoveFlowDefinitionFailed + doc.path); return; }
    if (!internal && doc.dirty && QMessageBox::warning(this, strings::UnsavedChangesTitle, strings::RemoveUnsavedFlowConfirmation, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    documents_.erase(documents_.begin() + index); current_ = -1;
    if (documents_.empty()) newFlow(); else selectFlow(qMin(index, int(documents_.size()) - 1));
    persist();
}
void Window::openFlow() {
    auto path = QFileDialog::getOpenFileName(this, strings::OpenFlowTitle, {}, strings::FlowFileFilter); if (path.isEmpty()) return;
    for (int i = 0; i < int(documents_.size()); ++i) if (documents_[size_t(i)].path == path) { selectFlow(i); return; }
    try {
        auto flow = Flow::load(path);
        // Invalid definitions remain editable, but dangling edges are not safe to render.
        for (auto &e : flow.edges) if (!flow.nodes.contains(e.from) || !flow.nodes.contains(e.to) || e.input >= flow.inputCount(flow.nodes.at(e.to)) || e.output >= flow.outputCount(flow.nodes.at(e.from)))
            throw std::runtime_error(strings::InvalidDrawingConnection);
        documents_.push_back({flow, path, false}); selectFlow(int(documents_.size()) - 1); persist();
    } catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); }
}
bool Window::saveFlow(bool saveAs) {
    if (current_ < 0) return false;
    auto &doc = documents_[size_t(current_)]; auto path = doc.path;
    if (saveAs) { path = QFileDialog::getSaveFileName(this, strings::SaveFlowTitle, path, strings::FlowFileFilter); if (path.isEmpty()) return false; }
    if (path != doc.path) {
        QStringList ids; for (const auto &project : projects_) if (project.flowPath == doc.path) ids.push_back(project.historyId);
        if (!confirmStopProjects(ids)) return false;
    }
    try {
        editGraph_->flow.save(path);
        for (auto &p : projects_) if (p.flowPath == doc.path) p.flowPath = path;
        if (path != doc.path) {
            const auto old = doc.path;
            for (auto &[id, node] : editGraph_->flow.nodes) if (isHostOperation(node.kind) && node.targetFlow == old) { node.targetFlow = path; if (auto *form = editGraph_->form(id)) form->setNode(node); }
            for (auto &other : documents_) for (auto &[id, node] : other.flow.nodes) if (isHostOperation(node.kind) && node.targetFlow == old) { node.targetFlow = path; other.dirty = true; }
            editGraph_->flow.save(path);
        }
        doc.path = path; doc.flow = editGraph_->flow; doc.dirty = false; doc.undo->setClean(); doc.savedNextNodeId = editGraph_->flow.nextNodeId;
        refreshActivities(); setWindowTitle(strings::WindowTitlePrefix + doc.flow.name); persist(); statusBar()->showMessage(strings::SavedFlowMessage + path, time::StatusSavedMilliseconds); return true;
    } catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); return false; }
}
void Window::validateFlow() {
    auto errors = editGraph_->flow.validate();
    errors.append(modelSelectionErrors(editGraph_->flow));
    if (errors.isEmpty()) QMessageBox::information(this, strings::ValidateFlowTitle, strings::NoValidationErrors);
    else reportError(errors.join('\n'));
}
void Window::refreshProjects() {
    const int previous = project_;
    QSignalBlocker block(projectList_); projectList_->clear(); project_ = -1;
    if (current_ < 0) { refreshUserInputPanel(); updateExecutionUi(); return; }
    auto path = documents_[size_t(current_)].path;
    for (int i = 0; i < int(projects_.size()); ++i) if (projects_[size_t(i)].flowPath == path) {
        auto *item = new QListWidgetItem(projects_[size_t(i)].name, projectList_); item->setData(Qt::UserRole, i); item->setToolTip(projects_[size_t(i)].directory);
        auto *row = new QWidget; row->setObjectName(strings::ProjectRow); row->setStyleSheet(strings::ProjectRowStyle); auto *layout = new QHBoxLayout(row); layout->setContentsMargins(metrics::SpaceNormal, metrics::SpaceTiny, metrics::SpaceTiny, metrics::SpaceTiny); layout->setSpacing(metrics::SpaceSmall);
        auto *nameLabel = new ProjectNameLabel(projects_[size_t(i)].name); nameLabel->setObjectName(strings::ProjectName); nameLabel->setTextFormat(Qt::PlainText); nameLabel->setMinimumWidth(0); nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); nameLabel->setToolTip(projects_[size_t(i)].name); layout->addWidget(nameLabel, 1);
        for (const auto &name : {strings::ProjectRunningIcon, strings::ProjectInputWaitingIcon}) {
            auto *indicator = new ProjectStatusLabel; indicator->setObjectName(name); indicator->setFixedSize(metrics::ActivityIndicatorExtent, metrics::ActivityIndicatorExtent); indicator->setAlignment(Qt::AlignCenter); indicator->setStyleSheet(strings::ActivityIconStyle); indicator->setAutoFillBackground(false); layout->addWidget(indicator);
        }
        auto *more = new QToolButton; more->setText(strings::MoreActionsSymbol); more->setObjectName(strings::ProjectOpenMenu);
        more->setToolTip(strings::OpenDirectoryTooltip); more->setAccessibleName(strings::OpenProjectDirectoryTooltip); layout->addWidget(more);
        auto *menu = new QMenu(more); more->setMenu(menu); more->setPopupMode(QToolButton::InstantPopup);
        connect(menu, &QMenu::aboutToShow, this, [this, menu, i] {
            menu->clear();
            for (const auto &opener : detectProjectOpeners()) {
                menu->addAction(opener.label, this, [this, i, opener] {
                    if (i >= int(projects_.size())) return;
                    QString error; if (!openProjectDirectory(opener, projects_[size_t(i)].directory, &error)) reportError(error);
                });
            }
        });
        auto *remove = new QToolButton; remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
        remove->setObjectName(strings::RemoveProject); remove->setToolTip(strings::RemoveProjectTooltip); remove->setAccessibleName(strings::RemoveProjectTitle); layout->addWidget(remove);
        item->setText(strings::Empty); item->setSizeHint(row->sizeHint()); projectList_->setItemWidget(item, row);
        connect(remove, &QToolButton::clicked, this, [this, i] {
            if (i >= int(projects_.size()) || !confirmStopProjects({projects_[size_t(i)].historyId})) return;
            const auto selected = project_ >= 0 ? projects_[size_t(project_)].historyId : QString();
            projects_.erase(projects_.begin() + i); project_ = -1;
            for (size_t index = 0; index < projects_.size(); ++index) if (projects_[index].historyId == selected) project_ = int(index);
            syncCoordinatorRegistry(); refreshProjects(); persist();
        });
    }
    if (projectList_->count()) {
        int row = 0;
        for (int i = 0; i < projectList_->count(); ++i) if (projectList_->item(i)->data(Qt::UserRole).toInt() == previous) { row = i; project_ = previous; break; }
        projectList_->setCurrentRow(row);
        const int index = projectList_->item(row)->data(Qt::UserRole).toInt();
        selectProject(index);
    }
    else { workGraph_->replace(editGraph_->flow); workSelected_ = 0; selectWorkNode(0); status_->setText(strings::AddProjectHint); }
    refreshProjectStatusIcons();
    updateExecutionUi();
    refreshUserInputPanel();
}
void Window::refreshProjectStatusIcons() {
    static const auto runningIcon = projectStatusIcon(false), waitingIcon = projectStatusIcon(true);
    for (int rowIndex = 0; rowIndex < projectList_->count(); ++rowIndex) {
        auto *item = projectList_->item(rowIndex); const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= int(projects_.size())) continue;
        auto *row = projectList_->itemWidget(item); if (!row) continue;
        const auto &project = projects_[size_t(index)]; const bool running = coordinator_->running(project.historyId);
        const auto pending = std::count_if(inputPrompts_.begin(), inputPrompts_.end(), [&](const InputPrompt &prompt) { return prompt.projectId == project.historyId; });
        QStringList states;
        auto update = [row](const char *name, bool active, const QIcon &icon, const QString &text) {
            auto *label = row->findChild<QLabel *>(name); if (!label) return;
            label->setProperty(strings::Active, active); label->setToolTip(active ? text : QString()); label->setAccessibleName(active ? text : QString());
            if (active) label->setPixmap(icon.pixmap(QSize(metrics::ActivityIconExtent, metrics::ActivityIconExtent), label->devicePixelRatioF())); else label->clear();
            label->setVisible(active);
        };
        const QString waitingText = QString(strings::PendingInputCount).arg(pending);
        update(strings::ProjectRunningIcon, running, runningIcon, strings::ProjectRunningHint);
        update(strings::ProjectInputWaitingIcon, pending > 0, waitingIcon, waitingText);
        if (running) states << strings::ProjectRunningHint;
        if (pending > 0) states << waitingText;
        const auto state = states.isEmpty() ? QString(strings::ProjectIdleHint) : states.join(strings::StatusSeparator);
        row->setAccessibleName(project.name + strings::ProjectTooltipSeparator + state); item->setData(Qt::AccessibleTextRole, project.name + strings::ProjectTooltipSeparator + state);
        item->setToolTip(project.directory + '\n' + state); row->setToolTip(project.directory + '\n' + state);
    }
}
void Window::selectProject(int index) {
    if (index < 0 || index >= int(projects_.size())) return;
    const bool sameProject = project_ == index;
    project_ = index; auto &p = projects_[size_t(index)];
    auto displayFlow = p.runFlow ? *p.runFlow : editGraph_->flow;
    for (const auto &[id, position] : p.workPositions) if (displayFlow.nodes.contains(id)) displayFlow.nodes[id].position = position;
    const bool replace = !sameProject || workGraph_->flow.toJson() != displayFlow.toJson();
    if (replace) { workGraph_->replace(displayFlow); workView_->centerScene(); }
    for (auto &[id, r] : p.results) if (workGraph_->nodeExists(id)) workGraph_->states[id] = r.state;
    for (auto &[id, n] : workGraph_->flow.nodes) emit workGraph_->nodeUpdated(id);
    status_->setText(p.directory); updateExecutionUi();
    if (sameProject && (workSelected_ == 0 || workGraph_->nodeExists(workSelected_))) {
        workGraph_->setHighlightedNode(workSelected_); refreshWorkHistory();
    } else selectWorkNode(0);
    refreshUserInputPanel();
}
void Window::selectWorkNode(Id id) {
    workSelected_ = id; selectedHistory_.clear(); followLatestHistory_ = true;
    workGraph_->setHighlightedNode(id);
    refreshWorkHistory();
}
void Window::refreshWorkHistory() {
    std::vector<const NodeExecution *> entries;
    if (project_ >= 0 && workGraph_->nodeExists(workSelected_)) for (auto it = projects_[size_t(project_)].history.rbegin(); it != projects_[size_t(project_)].history.rend(); ++it)
        if (it->node == workSelected_) entries.push_back(&*it);
    const int scroll = historyList_->verticalScrollBar()->value();
    QSignalBlocker block(historyList_);
    bool rebuild = historyList_->count() != int(entries.size());
    if (!rebuild) for (int row = 0; row < int(entries.size()); ++row)
        if (historyList_->item(row)->data(Qt::UserRole).toString() != entries[size_t(row)]->id) { rebuild = true; break; }
    if (rebuild) historyList_->clear();
    int selectedRow = -1;
    for (int row = 0; row < int(entries.size()); ++row) {
        const auto &entry = *entries[size_t(row)];
        auto *item = rebuild ? new QListWidgetItem(historyList_) : historyList_->item(row);
        item->setData(Qt::UserRole, entry.id);
        const QString title = QString(strings::ExecutionNumberText).arg(entry.executionNumber);
        const bool continued = entry.retainContext && entry.contextResumed;
        QString details = entry.agent.isEmpty() ? QString(strings::InternalExecution) : entry.agent;
        if (entry.agent == strings::Codex || entry.agent == strings::ClaudeCode || entry.agent == strings::OpenCode) {
            QString effort = entry.effort.isEmpty() ? QString(strings::DefaultEffort) : entry.effort;
            if (!effort.isEmpty()) effort[0] = effort[0].toUpper();
            details += strings::Space + (entry.model.isEmpty() ? strings::MissingExecutionMetadata : entry.model) + strings::Space + effort;
        }
        item->setText(title + (continued ? strings::ContinuedContextSuffix : strings::Empty) + strings::Newline + details);
        item->setData(Qt::UserRole + 1, title.toHtmlEscaped() + (continued ? strings::ContinuedContextHtml : strings::Empty) + strings::HtmlLineBreak + details.toHtmlEscaped());
        item->setToolTip(strings::ExecutionStartedPrefix + entry.startedAt.toLocalTime().toString(Qt::ISODateWithMs) + strings::ExecutionFinishedPrefix +
            (entry.finishedAt.isValid() ? entry.finishedAt.toLocalTime().toString(Qt::ISODateWithMs) : entry.state == State::Running ? strings::Running : strings::InterruptedExecution) + strings::ExecutionStatePrefix + stateName(entry.state));
        if (entry.id == selectedHistory_) selectedRow = row;
    }
    if ((followLatestHistory_ || selectedRow < 0) && !entries.empty()) selectedRow = 0;
    historyList_->setCurrentRow(selectedRow);
    selectedHistory_ = selectedRow >= 0 ? entries[size_t(selectedRow)]->id : QString();
    if (followLatestHistory_) historyList_->scrollToTop(); else historyList_->verticalScrollBar()->setValue(scroll);
    QSignalBlocker followBlock(followHistoryButton_); followHistoryButton_->setChecked(followLatestHistory_); followHistoryButton_->setEnabled(!entries.empty());
    showWorkResult();
}
void Window::showWorkResult() {
    const NodeExecution *execution = nullptr;
    if (project_ >= 0 && !selectedHistory_.isEmpty()) {
        const auto &history = projects_[size_t(project_)].history;
        auto it = std::find_if(history.begin(), history.end(), [this](const auto &entry) { return entry.id == selectedHistory_; });
        if (it != history.end()) execution = &*it;
    }
    const QString key = execution ? execution->id : QString(strings::CurrentExecutionKey).arg(project_).arg(workSelected_);
    auto render = [this, &key](const QString &log, State state, const Json *output = nullptr, bool structured = false, const QString &directory = QString(), bool plainLog = false) {
        log_->setProgress(log, key, state, output, structured, directory, plainLog);
        if (output) output_->setValue(*output, true, structured || log_->finalIsJson()); else output_->setPlainText({});
    };
    if (execution) {
        nodeStatus_->setText(execution->title + strings::ProjectTooltipSeparator + stateName(execution->state) + strings::Newline + execution->startedAt.toLocalTime().toString(strings::YyyyMMDdHHMmSsZzz));
        render(execution->log, execution->state, execution->state == State::Succeeded ? &execution->output : nullptr, execution->structuredOutput, execution->directory, execution->agent == strings::Python || execution->agent == strings::NodeJs || execution->agent == strings::PowerShell || execution->agent == strings::Sh); return;
    }
    if (!workGraph_->nodeExists(workSelected_)) { render({}, State::Pending); nodeStatus_->setText(strings::SelectNodeHint); return; }
    const auto &node = workGraph_->flow.nodes.at(workSelected_);
    const auto title = node.title.isEmpty() ? kindName(node.kind) : node.title;
    if (project_ < 0 || !projects_[size_t(project_)].results.contains(workSelected_)) { render({}, State::Pending); nodeStatus_->setText(title + strings::WaitingSuffix); return; }
    const auto &r = projects_[size_t(project_)].results.at(workSelected_);
    nodeStatus_->setText(title + strings::ProjectTooltipSeparator + stateName(r.state) + QString(strings::ExecutionCountSuffix).arg(r.executions));
    render(r.log, r.state, r.state == State::Succeeded ? &r.output : nullptr, nodeDefinition(node.kind).structured(node), {}, nodePresentation(node.kind).processOutput);
}
void Window::refreshUserInputPanel() {
    const auto previous = activeInput_;
    for (const auto &prompt : inputPrompts_) if (prompt.request.id == previous) selectedInputs_[prompt.projectId] = previous;
    const QString projectId = project_ >= 0 && project_ < int(projects_.size()) ? projects_[size_t(project_)].historyId : QString();
    const QSignalBlocker block(inputSelector_);
    inputSelector_->clear();
    std::vector<const InputPrompt *> visible;
    for (const auto &prompt : inputPrompts_) {
        if (prompt.projectId != projectId || !coordinator_->running(projectId)) continue;
        QString label = kindName(Kind::UserInput);
        for (const auto &project : projects_) if (project.historyId == prompt.projectId) {
            const Flow *flow = project.runFlow ? &*project.runFlow : nullptr;
            if (!flow) for (const auto &document : documents_) if (document.path == project.flowPath) { flow = &document.flow; break; }
            if (flow && flow->nodes.contains(prompt.request.node)) { const auto &node = flow->nodes.at(prompt.request.node); if (!node.title.isEmpty()) label = node.title; }
            break;
        }
        inputSelector_->addItem(label, prompt.request.id); visible.push_back(&prompt);
    }
    int selected = inputSelector_->findData(selectedInputs_.contains(projectId) ? selectedInputs_.at(projectId) : QString());
    if (selected < 0 && !visible.empty()) selected = 0;
    inputSelector_->setCurrentIndex(selected); activeInput_ = inputSelector_->currentData().toString();
    while (auto *item = inputButtons_->takeAt(0)) { if (auto *widget = item->widget()) { widget->hide(); widget->deleteLater(); } delete item; }
    inputShortcutHint_ = nullptr;
    if (selected < 0) { inputPanel_->hide(); const QSignalBlocker textBlock(inputText_); inputText_->clear(); inputMessage_->setPlainText({}); return; }
    selectedInputs_[projectId] = activeInput_;
    const auto &prompt = *visible.at(size_t(selected));
    inputMessage_->setValue(prompt.request.message, true, !prompt.request.message.is_string());
    inputMessage_->setVisible(!prompt.request.message.is_string() || !QString::fromStdString(prompt.request.message.get<std::string>()).trimmed().isEmpty());
    if (inputText_->toPlainText() != prompt.draft) { const QSignalBlocker textBlock(inputText_); inputText_->setPlainText(prompt.draft); }
    inputText_->setEnabled(true);
    for (qsizetype index = 0; index < prompt.request.choices.size(); ++index) {
        auto *choice = new QPushButton(prompt.request.choices[index]); choice->setObjectName(strings::UserInputChoice); choice->setProperty(strings::ChoiceIndex, index); inputButtons_->addWidget(choice);
        connect(choice, &QPushButton::clicked, this, [this, id = prompt.request.id, projectId, index] {
            if (id != activeInput_ || project_ < 0 || project_ >= int(projects_.size()) || projects_[size_t(project_)].historyId != projectId) return;
            if (coordinator_->submitUserInput(id, inputText_->toPlainText(), unsigned(index))) {
                inputText_->setEnabled(false);
                for (int i = 0; i < inputButtons_->count(); ++i) if (auto *widget = inputButtons_->itemAt(i)->widget()) widget->setEnabled(false);
                updateInputShortcutHint();
            }
        });
    }
    inputShortcutHint_ = new QLabel(strings::InputShortcutPrefix + prompt.request.choices.value(0)); inputShortcutHint_->setObjectName(strings::UserInputShortcutHint); inputShortcutHint_->setTextFormat(Qt::PlainText);
    inputShortcutHint_->setStyleSheet(strings::InputShortcutStyle); inputButtons_->addWidget(inputShortcutHint_); updateInputShortcutHint();
    inputButtons_->addStretch(); const bool hidden = inputPanel_->isHidden(); inputPanel_->show();
    if (hidden) workInputSplit_->setSizes({qMax(metrics::WorkGraphMinimumHeight, workInputSplit_->height() - inputPaneHeight_ - workInputSplit_->handleWidth()), inputPaneHeight_});
    if (previous != activeInput_) focusUserInputNode();
}
void Window::focusUserInputNode() {
    if (activeInput_.isEmpty() || project_ < 0 || project_ >= int(projects_.size())) return;
    const auto id = activeInput_, projectId = projects_[size_t(project_)].historyId;
    for (const auto &prompt : inputPrompts_) if (prompt.request.id == id && prompt.projectId == projectId) {
        const auto node = prompt.request.node;
        QTimer::singleShot(0, this, [this, id, projectId, node] {
            if (activeInput_ != id || project_ < 0 || project_ >= int(projects_.size()) || projects_[size_t(project_)].historyId != projectId || !workView_->isVisible() || !workGraph_->nodeExists(node)) return;
            if (workSelected_ != node) selectWorkNode(node);
            static_cast<GraphView *>(workView_)->zoomToNode(node);
        });
        break;
    }
}
void Window::recordNodeExecution(const QString &projectId, const NodeExecution &value) {
    auto found = std::find_if(projects_.begin(), projects_.end(), [&](const Project &p) { return p.historyId == projectId; });
    if (found == projects_.end()) return;
    auto &project = *found; auto entry = value;
    auto it = std::find_if(project.history.begin(), project.history.end(), [&](const auto &old) { return old.id == entry.id; });
    const bool added = it == project.history.end();
    entry.sequence = added ? (project.history.empty() ? 1 : project.history.back().sequence + 1) : it->sequence;
    if (added) project.history.push_back(entry); else *it = entry;
    if (!smoke_) {
        pendingHistory_[entry.id] = {dataDir_ + strings::HistoryDirectorySuffix + project.historyId, entry};
        if (added || entry.state != State::Running) flushHistory();
        else if (!historySaveTimer_->isActive()) historySaveTimer_->start(time::HistorySaveMilliseconds);
    }
    if (project_ >= 0 && projects_[size_t(project_)].historyId == projectId && (workSelected_ == 0 || workSelected_ == entry.node)) refreshWorkHistory();
}
void Window::flushHistory() {
    historySaveTimer_->stop(); auto pending = std::move(pendingHistory_); pendingHistory_.clear();
    QString error;
    for (const auto &[id, record] : pending) {
        try { saveNodeExecution(record.first, record.second); }
        catch (const std::exception &e) { pendingHistory_[id] = record; error = QString::fromUtf8(e.what()); }
    }
    if (!pendingHistory_.empty()) {
        historySaveTimer_->start(time::HistoryRetryMilliseconds);
        if (!historySaveFailed_) { historySaveFailed_ = true; reportError(error); }
    } else historySaveFailed_ = false;
}
void Window::addProject() {
    if (current_ < 0) return;
    auto directory = QFileDialog::getExistingDirectory(this, strings::ProjectDirectoryTitle); if (directory.isEmpty()) return;
    projects_.push_back({QFileInfo(directory).fileName(), directory, documents_[size_t(current_)].path, {}});
    syncCoordinatorRegistry(); refreshProjects(); projectList_->setCurrentRow(projectList_->count() - 1); persist();
}
void Window::executeFlow() {
    if (project_ < 0) { status_->setText(strings::SelectProjectHint); return; }
    if (coordinator_->running(projects_[size_t(project_)].historyId)) { status_->setText(strings::ProjectAlreadyRunning); return; }
    auto errors = editGraph_->flow.validate(); if (!errors.isEmpty()) { reportError(errors.join('\n')); return; }
    errors = modelSelectionErrors(editGraph_->flow); if (!errors.isEmpty()) { reportError(errors.join('\n')); return; }
    persist(); // Preserve the project's history identity before the first record is written.
    syncCoordinatorRegistry();
    coordinator_->flows[documents_[size_t(current_)].path] = editGraph_->flow;
    const auto &p = projects_[size_t(project_)];
    const Coordinator::Project target{p.historyId, p.flowPath, p.directory};
    if (!coordinator_->start(target, runner_->running() ? nullptr : runner_)) {
        updateExecutionUi();
        reportError(strings::CannotStartFlow);
    }
}
void Window::settings() {
    QDialog dialog(this); dialog.setWindowTitle(strings::CliSettingsTitle); dialog.resize(metrics::CliDialogWidth, metrics::CliDialogHeight);
    auto *layout = new QVBoxLayout(&dialog); auto *form = new QFormLayout;
    auto add = [this, form](const QString &label, const QString &value) {
        auto *row = new QWidget; auto *layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(value); layout->addWidget(edit, 1);
        button(strings::Browse, layout, [this, edit] { auto p = QFileDialog::getOpenFileName(this, strings::CliExecutableTitle, {}, strings::ExecutableFileFilter); if (!p.isEmpty()) edit->setText(p); });
        form->addRow(label, row); return edit;
    };
    auto *codex = add(strings::Codex, config_.codex), *claude = add(strings::ClaudeCode, config_.claude), *opencode = add(strings::OpenCode, config_.opencode);
    layout->addLayout(form);
    auto *hint = new QLabel(strings::CliPermissionsHint); hint->setWordWrap(true); layout->addWidget(hint);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        config_ = {codex->text(), claude->text(), opencode->text()};
        syncCoordinatorRegistry(); persist(); if (!smoke_) { modelCatalog_->setConfig(config_); modelCatalog_->refreshAll(true); }
    }
}
void Window::reportError(const QString &message) { if (!smoke_) QMessageBox::warning(this, strings::Error, message); statusBar()->showMessage(message, time::StatusLongMilliseconds); }
void Window::persist() {
    if (smoke_) return;
    Json paths = Json::array(), projects = Json::array();
    for (auto &d : documents_) paths.push_back(str(d.path));
    for (auto &p : projects_) projects.push_back({{strings::Name, str(p.name)}, {strings::Directory, str(p.directory)}, {strings::Flow, str(p.flowPath)}, {strings::HistoryId, str(p.historyId)}});
    Json j = {{strings::Flows, paths}, {strings::Projects, projects}, {strings::CodexExecutable, str(config_.codex)}, {strings::ClaudeExecutable, str(config_.claude)}, {strings::OpenCodeExecutable, str(config_.opencode)}};
    QSaveFile file(dataDir_ + strings::WorkspaceFileSuffix); auto bytes = QByteArray::fromStdString(j.dump(defaults::JsonIndent));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) reportError(strings::SaveWorkspaceFailed + file.errorString());
}
void Window::restore() {
    QFile file(dataDir_ + strings::WorkspaceFileSuffix); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) { reportError(file.errorString()); return; }
    try {
        auto j = Json::parse(file.readAll().toStdString());
        config_ = {qs(j.at(strings::CodexExecutable)), qs(j.at(strings::ClaudeExecutable)), qs(j.at(strings::OpenCodeExecutable))};
#ifdef Q_OS_WIN
        // Migrate only our old bare default, never a user-specified executable path.
        if (config_.opencode == strings::OpenCodeExecutable) config_.opencode = strings::OpenCodeWindowsExecutable;
#endif
        for (auto &path : j.at(strings::Flows)) {
            try {
                auto flow = Flow::load(qs(path));
                for (auto &e : flow.edges) if (!flow.nodes.contains(e.from) || !flow.nodes.contains(e.to) || e.input >= flow.inputCount(flow.nodes.at(e.to)) || e.output >= flow.outputCount(flow.nodes.at(e.from))) throw std::runtime_error(strings::InvalidConnectionTitle);
                documents_.push_back({flow, qs(path), false});
            } catch (const std::exception &e) { reportError(qs(path) + strings::ColonSpace + QString::fromUtf8(e.what())); }
        }
        for (auto &p : j.at(strings::Projects)) {
            Project project{qs(p.at(strings::Name)), qs(p.at(strings::Directory)), qs(p.at(strings::Flow)), {}};
            if (p.contains(strings::HistoryId) && p.at(strings::HistoryId).is_string()) {
                const auto uuid = QUuid(qs(p.at(strings::HistoryId)));
                if (!uuid.isNull()) project.historyId = uuid.toString(QUuid::WithoutBraces);
            }
            QStringList errors; project.history = loadNodeExecutions(dataDir_ + strings::HistoryDirectorySuffix + project.historyId, &errors);
            for (const auto &entry : project.history) {
                auto &result = project.results[entry.node]; ++result.executions;
                result.state = entry.state; result.log = entry.log; result.output = entry.output;
            }
            projects_.push_back(std::move(project));
            if (!errors.isEmpty()) reportError(strings::LoadHistoryFailed + errors.join('\n'));
        }
    } catch (const std::exception &e) { reportError(strings::LoadSettingsFailed + QString::fromUtf8(e.what())); }
}
void Window::closeEvent(QCloseEvent *event) {
    if (!quitting_) {
        event->ignore();
        if (smoke_ || QSystemTrayIcon::isSystemTrayAvailable()) hide();
        else statusBar()->showMessage(strings::TrayUnavailableNotice, time::StatusLongMilliseconds);
        return;
    }
    if (busy()) {
        showWindow();
        if (QMessageBox::warning(this, strings::Quit, strings::QuitRunningFlowsConfirmation, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
        coordinator_->cancelAll();
    }
    flushHistory();
    if (!pendingHistory_.empty() && QMessageBox::warning(this, strings::SaveHistoryTitle, strings::QuitUnsavedHistoryConfirmation, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    // Preserve every modified flow, not only the currently selected activity.
    for (auto &doc : documents_) if (doc.dirty && !smoke_) {
        try { doc.flow.save(doc.path); doc.dirty = false; }
        catch (const std::exception &e) { reportError(QString::fromUtf8(e.what())); event->ignore(); return; }
    }
    persist(); event->accept();
}
bool Window::smokeTest() {
    auto id = editGraph_->addNode(strings::Agent); if (id == QtNodes::InvalidNodeId) return false;
    const auto end = editGraph_->addNode(strings::End); if (end == QtNodes::InvalidNodeId) return false;
    selectNode(id); showText(strings::Prompt); text_->setPlainText(strings::SayHello);
    editGraph_->addConnection({1, 0, id, 0}); editGraph_->addConnection({id, 0, end, 0});
    editGraph_->addConnection({id, 0, id, 0});
    if (!editGraph_->connectionExists({id, 0, id, 0})) return false;
    editGraph_->deleteConnection({id, 0, id, 0});
    if (!editGraph_->flow.validate().isEmpty()) return false;
    workGraph_->replace(editGraph_->flow); tabs_->setCurrentIndex(1); selectWorkNode(id);
    const auto count = workGraph_->flow.nodes.size();
    if (workGraph_->addNode(strings::Agent) != QtNodes::InvalidNodeId || workGraph_->flow.nodes.size() != count) return false;
    if (workGraph_->deleteNode(id) || !workGraph_->setNodeData(id, QtNodes::NodeRole::Position, QPointF(1, 1))) return false;
    return true;
}
}
