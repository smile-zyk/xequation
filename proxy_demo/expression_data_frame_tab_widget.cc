#include "expression_data_frame_tab_widget.h"

#include "expression_data_frame_view.h"

#include <QInputDialog>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

namespace xresults
{
namespace gui
{

using namespace xequation;

// =========================================================================
// ExpressionDataFrameTabWidget
// =========================================================================

ExpressionDataFrameTabWidget::ExpressionDataFrameTabWidget(
    EquationManager &manager, QWidget *parent)
    : QTabWidget(parent), manager_(manager)
{
    setTabsClosable(true);
    setDocumentMode(true);
    connect(this, &QTabWidget::tabCloseRequested,
            this, &ExpressionDataFrameTabWidget::CloseTab);
    connect(tabBar(), &QTabBar::tabBarDoubleClicked,
            this, &ExpressionDataFrameTabWidget::OnTabLabelDoubleClicked);

    reeval_timer_ = new QTimer(this);
    reeval_timer_->setSingleShot(true);
    reeval_timer_->setInterval(0);
    connect(reeval_timer_, &QTimer::timeout,
            this, &ExpressionDataFrameTabWidget::OnReevalTimer);
}

ExpressionDataFrameTabWidget::~ExpressionDataFrameTabWidget()
{
    // Unregister every remaining registered expression (equation tabs hold a
    // group id; RemoveExpression is a no-op for them).  The view widgets are
    // children and get destroyed by ~QTabWidget.
    for (const TabData &tab : tabs_)
    {
        if (tab.kind == ObjectKind::kExpression)
        {
            manager_.RemoveExpression(tab.object_id);
        }
    }
}

// ---- tab lifecycle ------------------------------------------------------

int ExpressionDataFrameTabWidget::FindTabIndex(const ObjectId &object_id) const
{
    const auto it = object_to_index_.find(object_id);
    if (it == object_to_index_.end())
    {
        return -1;
    }
    return it->second;
}

int ExpressionDataFrameTabWidget::OpenTab()
{
    // Tab contents: a single table view; errors are rendered as an overlay
    // inside the view itself (ExpressionDataFrameView::SetError).
    auto *view = new ExpressionDataFrameView(manager_, this);

    const int index = addTab(view, QString());
    tabs_.emplace_back();
    tabs_.back().view = view;

    // Pin button as the tab's left-side widget: toggling it pins / unpins the
    // tab (pinned tabs survive deselection and are ordered first).
    auto *pin_button = new QToolButton(tabBar());
    pin_button->setCheckable(true);
    pin_button->setText(QStringLiteral("○"));
    pin_button->setToolTip(QStringLiteral("Pin this tab: keep it when deselected"));
    pin_button->setAutoRaise(true);
    pin_button->setCursor(Qt::PointingHandCursor);
    tabs_.back().pin_button = pin_button;
    tabBar()->setTabButton(index, QTabBar::LeftSide, pin_button);
    // Resolve the tab index by button identity at click time: tabs may have
    // been re-ordered by pin/unpin since the button was created.
    connect(pin_button, &QToolButton::toggled, this, [this, pin_button](bool checked) {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
        {
            if (tabs_[static_cast<std::size_t>(i)].pin_button == pin_button)
            {
                OnPinButtonClicked(i, checked);
                return;
            }
        }
    });

    // The object->index mapping is (re)built by RebuildKeyToIndex once the
    // caller has set the tab's object id.
    return static_cast<int>(tabs_.size()) - 1;
}

void ExpressionDataFrameTabWidget::CloseTabInternal(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }

    const TabData tab = tabs_[static_cast<std::size_t>(index)];
    if (tab.kind == ObjectKind::kExpression)
    {
        // A registered expression is released from the manager (this also
        // removes its graph node and dependency edges).
        manager_.RemoveExpression(tab.object_id);
    }
    UnregisterDependencies(tab.object_id, tab.dependencies);
    object_to_index_.erase(tab.object_id);
    dirty_keys_.erase(std::remove_if(dirty_keys_.begin(), dirty_keys_.end(),
                                     [&tab](const ObjectId &k) { return k == tab.object_id; }),
                      dirty_keys_.end());

    tabs_.erase(tabs_.begin() + index);
    removeTab(index);   // deletes the view widget (and the pin button)
    RebuildKeyToIndex();
}

void ExpressionDataFrameTabWidget::CloseTab(int index)
{
    if (tabs_.size() <= 1)
    {
        // Keep at least one tab (like ExpressionWatch keeps the placeholder row).
        ClearAll();
        return;
    }
    CloseTabInternal(index);
}

void ExpressionDataFrameTabWidget::ClearAll()
{
    // Remove all tabs; leave a single empty one.
    while (tabs_.size() > 1)
    {
        CloseTabInternal(static_cast<int>(tabs_.size()) - 1);
    }
    if (tabs_.size() == 1)
    {
        CloseTabInternal(0);
    }
    dirty_keys_.clear();
}

void ExpressionDataFrameTabWidget::FillTab(ExpressionDataFrameView *view,
                                         const EquationValue &value)
{
    view->SetValue(value);
}

// ---- pinning / ordering ---------------------------------------------------

void ExpressionDataFrameTabWidget::MoveTab(int from, int to)
{
    if (from == to || from < 0 || from >= static_cast<int>(tabs_.size()) ||
        to < 0 || to >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    const TabData tab = tabs_[static_cast<std::size_t>(from)];
    tabs_.erase(tabs_.begin() + from);
    tabs_.insert(tabs_.begin() + to, tab);
    tabBar()->moveTab(from, to);
    RebuildKeyToIndex();
}

void ExpressionDataFrameTabWidget::RebuildKeyToIndex()
{
    object_to_index_.clear();
    for (std::size_t i = 0; i < tabs_.size(); ++i)
    {
        object_to_index_[tabs_[i].object_id] = static_cast<int>(i);
    }
}

void ExpressionDataFrameTabWidget::UpdatePinButton(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    if (!tab.pin_button)
    {
        return;
    }
    const bool checked = tab.pinned;
    // Avoid recursive toggled signal while syncing the visual state.
    QSignalBlocker blocker(tab.pin_button);
    tab.pin_button->setChecked(checked);
    tab.pin_button->setText(checked ? QStringLiteral("◉") : QStringLiteral("○"));
}

void ExpressionDataFrameTabWidget::OnPinButtonClicked(int index, bool checked)
{
    SetTabPinned(index, checked);
}

void ExpressionDataFrameTabWidget::SetTabPinned(int index, bool pinned)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.pinned = pinned;
    UpdatePinButton(index);

    // Keep the order: pinned tabs first (in pin order), then unpinned.
    if (pinned)
    {
        // Find the last pinned tab before us (exclusive); move right after it.
        int target = 0;
        for (int i = 0; i < index; ++i)
        {
            if (tabs_[static_cast<std::size_t>(i)].pinned)
            {
                target = i + 1;
            }
        }
        // Also stop before any pinned tab after us (they already precede us).
        MoveTab(index, target);
    }
    else
    {
        // Move to just after the last pinned tab.
        int target = 0;
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
        {
            if (tabs_[static_cast<std::size_t>(i)].pinned)
            {
                target = i + 1;
            }
        }
        MoveTab(index, std::min(target, static_cast<int>(tabs_.size()) - 1));
    }
}

void ExpressionDataFrameTabWidget::SetTabError(int index, const QString &message)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    if (!tab.view)
    {
        return;
    }
    // The error overlay lives inside the view itself (covers the table area).
    tab.view->SetError(message);
}

// ---- dependency registration -----------------------------------------------

void ExpressionDataFrameTabWidget::UnregisterDependencies(
    const ObjectId &object_id, const std::vector<std::string> &deps)
{
    for (const std::string &dep : deps)
    {
        auto it = deps_to_keys_.find(dep);
        if (it == deps_to_keys_.end())
        {
            continue;
        }
        auto &keys = it->second;
        keys.erase(std::remove(keys.begin(), keys.end(), object_id), keys.end());
        if (keys.empty())
        {
            deps_to_keys_.erase(it);
        }
    }
}

void ExpressionDataFrameTabWidget::RegisterDependencies(
    const ObjectId &object_id, const std::vector<std::string> &deps)
{
    // DataFrames come only from the REL engine; the dependency map keys are
    // equation names, which are engine-agnostic for refresh purposes.
    for (const std::string &dep : deps)
    {
        auto &keys = deps_to_keys_[dep];
        if (std::find(keys.begin(), keys.end(), object_id) == keys.end())
        {
            keys.push_back(object_id);
        }
    }
}

// ---- evaluation ----------------------------------------------------------

void ExpressionDataFrameTabWidget::EvaluateTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    const TabData &tab = tabs_[static_cast<std::size_t>(index)];

    if (tab.kind == ObjectKind::kExpression)
    {
        // Registered expression: read the cached value straight from the
        // manager (the kExpressionUpdated event that triggered us already
        // carries the fresh state; the first computation is triggered by
        // AddExpression).
        const Expression *expr = manager_.GetExpression(tab.object_id);
        if (expr && expr->result.status == ResultStatus::kSuccess)
        {
            SetTabError(index, QString());
            FillTab(tab.view, expr->result.value);
            setTabText(index, QString::fromStdString(tab.expression));
            return;
        }
        // Not ready / failed: show the error (the value may simply not be
        // computed yet -- e.g. its dependencies are not all defined).
        tab.view->Clear();
        const QString error_message =
            expr ? QString("%1  (%2)")
                       .arg(QString::fromStdString(expr->result.message))
                       .arg(QString::fromStdString(ResultStatusConverter::ToString(expr->result.status)))
                 : QStringLiteral("Expression not registered.");
        SetTabError(index, error_message);
        setTabText(index, QString::fromStdString(tab.expression));
        return;
    }

    // Equation tab: read the value directly (group id -> single-equation group).
    const EquationGroup *group = manager_.GetEquationGroup(tab.object_id);
    const Equation *equation = group ? group->FirstEquation() : nullptr;
    if (equation && equation->status() == ResultStatus::kSuccess)
    {
        SetTabError(index, QString());
        FillTab(tab.view, equation->GetValue());
        setTabText(index, QString::fromStdString(tab.expression));
        return;
    }
    tab.view->Clear();
    const QString error_message =
        equation ? QString("%1  (%2)")
                       .arg(QString::fromStdString(equation->message()))
                       .arg(QString::fromStdString(ResultStatusConverter::ToString(equation->status())))
                 : QStringLiteral("Equation does not exist.");
    SetTabError(index, error_message);
    setTabText(index, QString::fromStdString(tab.expression));
}

// ---- change handling -----------------------------------------------------

void ExpressionDataFrameTabWidget::MarkDirty(const ObjectId &object_id)
{
    if (std::find(dirty_keys_.begin(), dirty_keys_.end(), object_id) == dirty_keys_.end())
    {
        dirty_keys_.push_back(object_id);
    }
    ScheduleReeval();
}

void ExpressionDataFrameTabWidget::ScheduleReeval()
{
    if (reeval_scheduled_)
    {
        return;
    }
    reeval_scheduled_ = true;
    reeval_timer_->start();
}

void ExpressionDataFrameTabWidget::OnReevalTimer()
{
    reeval_scheduled_ = false;
    if (dirty_keys_.empty())
    {
        return;
    }

    const std::vector<ObjectId> keys = dirty_keys_;
    dirty_keys_.clear();

    for (const ObjectId &key : keys)
    {
        const int index = FindTabIndex(key);
        if (index >= 0)
        {
            EvaluateTab(index);
        }
    }
}

// ---- change routing ------------------------------------------------------

void ExpressionDataFrameTabWidget::OnEquationRemoving(const Equation *equation)
{
    // The value is about to disappear.  Equation tabs showing it are cleared;
    // watch-expression tabs are left to the post-removal re-evaluation
    // (OnEquationRemoved) which turns them into errors but keeps them open.
    MarkDirty(equation ? equation->group_id() : ObjectId());
}

void ExpressionDataFrameTabWidget::OnEquationRemoved(const std::string &equation_name)
{
    // Re-evaluate every watch expression that depended on the removed
    // equation: their registration is still alive (a missing dependency just
    // stays inactive), so refresh shows the NameError, keeping the tab open
    // watch-like.
    const auto it = deps_to_keys_.find(equation_name);
    if (it == deps_to_keys_.end())
    {
        return;
    }
    for (const ObjectId &object_id : it->second)
    {
        MarkDirty(object_id);
    }
}

void ExpressionDataFrameTabWidget::OnEquationUpdated(
    const Equation *equation, bitmask::bitmask<EquationUpdateFlag> flags
)
{
    if (!equation)
    {
        return;
    }
    // Refresh only when the new value is ready.
    if (!(flags & EquationUpdateFlag::kValue))
    {
        return;
    }

    // An equation tab whose group_id matches this equation is refreshed by
    // marking its id dirty.
    MarkDirty(equation->group_id());

    // Watch expressions that depend on this equation: their cached value is
    // re-computed by the manager via UpdateNode (kExpressionUpdated comes
    // separately), so just mark the dependent tabs dirty here too -- the
    // consolidated OnReevalTimer reads the fresh value.
    const auto it = deps_to_keys_.find(equation->name());
    if (it != deps_to_keys_.end())
    {
        for (const ObjectId &object_id : it->second)
        {
            MarkDirty(object_id);
        }
    }
}

void ExpressionDataFrameTabWidget::OnExpressionUpdated(
    const Expression *expression, bitmask::bitmask<ExpressionUpdateFlag> /*flags*/
)
{
    if (!expression)
    {
        return;
    }
    // Watch-expression tab: refresh from the (already recomputed) cached value.
    MarkDirty(expression->id);
}

// ---- tabs: Add -----------------------------------------------------------

void ExpressionDataFrameTabWidget::AddEquation(const ObjectId &group_id, bool auto_pin)
{
    if (group_id.is_nil())
    {
        return;
    }

    // Duplicate object: focus existing tab instead of stacking a new one.
    const int existing_index = FindTabIndex(group_id);
    if (existing_index >= 0)
    {
        setCurrentIndex(existing_index);
        // Re-read the value so the tab is not stale (matching SyncSelection's
        // "re-selecting refreshes" semantics).
        MarkDirty(group_id);
        return;
    }

    const EquationGroup *group = manager_.GetEquationGroup(group_id);
    const Equation *equation = group ? group->FirstEquation() : nullptr;
    if (!equation)
    {
        return;
    }

    const int index = OpenTab();
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.kind = ObjectKind::kEquation;
    tab.object_id = group_id;
    tab.expression = equation->name();
    tab.dependencies = {tab.expression};   // self-register (value-ready refresh)
    setTabText(index, QString::fromStdString(tab.expression));

    RegisterDependencies(tab.object_id, tab.dependencies);
    RebuildKeyToIndex();
    EvaluateTab(index);
    setCurrentIndex(index);

    if (auto_pin)
    {
        const int pinned_index = FindTabIndex(group_id);
        SetTabPinned(pinned_index, true);
    }
}

void ExpressionDataFrameTabWidget::AddExpression(const ObjectId &expression_id,
                                               bool auto_pin)
{
    if (expression_id.is_nil())
    {
        return;
    }

    // Duplicate object: focus existing tab instead of stacking a new one.
    const int existing_index = FindTabIndex(expression_id);
    if (existing_index >= 0)
    {
        setCurrentIndex(existing_index);
        // Re-read the value so the tab is not stale.
        MarkDirty(expression_id);
        return;
    }

    const Expression *expr = manager_.GetExpression(expression_id);
    if (!expr)
    {
        return;
    }

    const int index = OpenTab();
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.kind = ObjectKind::kExpression;
    tab.object_id = expression_id;
    tab.expression = expr->content;
    tab.dependencies = expr->dependencies;
    setTabText(index, QString::fromStdString(tab.expression));

    RegisterDependencies(tab.object_id, tab.dependencies);
    RebuildKeyToIndex();

    // Trigger the first computation synchronously so the tab shows a value
    // immediately (AddExpression only registered + marked the graph node
    // dirty; the actual Eval happens here).  Afterwards the kExpressionUpdated
    // signal keeps the tab fresh -- EvaluateTab reads the cached value without
    // re-evaluating (no feedback loop).
    try
    {
        manager_.UpdateExpression(expression_id);
    }
    catch (const std::exception &)
    {
        // Keep the tab open; EvaluateTab renders the error state.
    }

    EvaluateTab(index);
    setCurrentIndex(index);

    if (auto_pin)
    {
        const int pinned_index = FindTabIndex(expression_id);
        SetTabPinned(pinned_index, true);
    }
}

void ExpressionDataFrameTabWidget::SyncSelection(
    const std::vector<std::string> &selected_equation_names)
{
    // 1. Close unpinned equation tabs whose equation is no longer selected.
    //    Watch-expression tabs and pinned tabs always survive.
    for (int i = static_cast<int>(tabs_.size()) - 1; i >= 0; --i)
    {
        TabData &tab = tabs_[static_cast<std::size_t>(i)];
        if (tab.pinned || tab.kind == ObjectKind::kExpression)
        {
            continue;
        }
        const bool still_selected =
            std::find(selected_equation_names.begin(), selected_equation_names.end(),
                      tab.expression) != selected_equation_names.end();
        if (!still_selected && manager_.IsEquationExist(tab.expression))
        {
            CloseTabInternal(i);
        }
    }

    // 2. Open / refresh equation tabs for the selected items (AddEquation
    //    focuses / refreshes the tab each call, matching "re-selecting
    //    refreshes").  Selection-driven tabs are not auto-pinned: they keep
    //    following the selection until the user pins them manually.
    for (const std::string &name : selected_equation_names)
    {
        const Equation *equation = manager_.GetEquation(name);
        if (equation)
        {
            AddEquation(equation->group_id(), /*auto_pin=*/false);
        }
    }
}

void ExpressionDataFrameTabWidget::OnTabLabelDoubleClicked(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }

    TabData &tab = tabs_[static_cast<std::size_t>(index)];

    // Only registered expressions are editable; equation tabs are identified
    // by their name and edited through the main editor.
    if (tab.kind != ObjectKind::kExpression)
    {
        return;
    }

    bool ok = false;
    const QString new_expression = QInputDialog::getText(
        this, QStringLiteral("Edit Expression"),
        QStringLiteral("Expression:"), QLineEdit::Normal,
        QString::fromStdString(tab.expression), &ok);
    if (!ok)
    {
        return;
    }
    const std::string trimmed = new_expression.trimmed().toStdString();
    if (trimmed.empty() || trimmed == tab.expression)
    {
        return;
    }

    // Re-register: remove the old watch expression, register the new one with
    // the manager and adopt its id.
    manager_.RemoveExpression(tab.object_id);
    UnregisterDependencies(tab.object_id, tab.dependencies);
    object_to_index_.erase(tab.object_id);

    ObjectId new_id;
    try
    {
        new_id = manager_.AddExpression(trimmed);
    }
    catch (const std::exception &)
    {
        new_id = ObjectId();
    }
    if (new_id.is_nil())
    {
        // Registration failed (parse error): restore the old tab state.
        tab.dependencies.clear();
        RegisterDependencies(tab.object_id, tab.dependencies);
        object_to_index_[tab.object_id] = index;
        RebuildKeyToIndex();
        EvaluateTab(index);
        return;
    }

    const Expression *new_expr = manager_.GetExpression(new_id);
    tab.object_id = new_id;
    tab.expression = trimmed;
    tab.dependencies = new_expr ? new_expr->dependencies : std::vector<std::string>{};
    RegisterDependencies(tab.object_id, tab.dependencies);

    object_to_index_[new_id] = index;
    setTabText(index, QString::fromStdString(trimmed));

    // Trigger the first computation of the edited expression synchronously.
    try
    {
        manager_.UpdateExpression(new_id);
    }
    catch (const std::exception &)
    {
        // Keep the tab open; EvaluateTab renders the error state.
    }

    EvaluateTab(index);
    RebuildKeyToIndex();

    // Edited expressions are auto-pinned like new ones (SetTabPinned re-orders
    // the tab into the pinned group; the index may change).
    const int pinned_index = FindTabIndex(new_id);
    SetTabPinned(pinned_index, true);
}

} // namespace gui
} // namespace xresults