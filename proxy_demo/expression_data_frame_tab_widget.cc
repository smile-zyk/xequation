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
    const EquationManager &manager, QWidget *parent)
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

ExpressionDataFrameTabWidget::~ExpressionDataFrameTabWidget() = default;

// ---- tab lifecycle ------------------------------------------------------

int ExpressionDataFrameTabWidget::FindTabIndex(const std::string &expression) const
{
    const auto it = expression_to_index_.find(expression);
    if (it == expression_to_index_.end())
    {
        return -1;
    }
    return it->second;
}

int ExpressionDataFrameTabWidget::OpenTab()
{
    // Tab contents: a single table view; errors are rendered as an overlay
    // inside the view itself (ExpressionDataFrameView::SetError).
    auto *view = new ExpressionDataFrameView(this);

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

    // The expression->index mapping is (re)built by RebuildKeyToIndex once the
    // caller has set the tab's expression.
    return static_cast<int>(tabs_.size()) - 1;
}

void ExpressionDataFrameTabWidget::CloseTabInternal(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }

    const TabData tab = tabs_[static_cast<std::size_t>(index)];
    UnregisterDependencies(tab.expression, tab.dependencies);
    expression_to_index_.erase(tab.expression);
    dirty_keys_.erase(std::remove_if(dirty_keys_.begin(), dirty_keys_.end(),
                                     [&tab](const std::string &k) { return k == tab.expression; }),
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
    expression_to_index_.clear();
    for (std::size_t i = 0; i < tabs_.size(); ++i)
    {
        expression_to_index_[tabs_[i].expression] = static_cast<int>(i);
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
    const std::string &expression, const std::vector<std::string> &deps)
{
    for (const std::string &dep : deps)
    {
        auto it = deps_to_keys_.find(dep);
        if (it == deps_to_keys_.end())
        {
            continue;
        }
        auto &keys = it->second;
        keys.erase(std::remove(keys.begin(), keys.end(), expression), keys.end());
        if (keys.empty())
        {
            deps_to_keys_.erase(it);
        }
    }
}

void ExpressionDataFrameTabWidget::RegisterDependencies(
    const std::string &expression, const std::vector<std::string> &deps)
{
    // DataFrames come only from the REL engine; the dependency map keys are
    // equation names, which are engine-agnostic for refresh purposes.
    for (const std::string &dep : deps)
    {
        auto &keys = deps_to_keys_[dep];
        if (std::find(keys.begin(), keys.end(), expression) == keys.end())
        {
            keys.push_back(expression);
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

    // Short-circuit: when the expression is exactly the name of an existing
    // equation, read its value directly -- no Eval re-evaluation needed
    // (the kValue event that triggered us already carries the fresh state).
    const Equation *existing = manager_.GetEquation(tab.expression);
    if (existing && existing->status() == ResultStatus::kSuccess)
    {
        SetTabError(index, QString());
        FillTab(tab.view, existing->GetValue());
        setTabText(index, QString::fromStdString(tab.expression));
        return;
    }

    // Otherwise evaluate the expression via the injected callback.
    const InterpretResult result = manager_.Eval(tab.expression);
    if (result.status != ResultStatus::kSuccess)
    {
        // Keep the tab; show the error in red below the (cleared) table.
        tab.view->Clear();
        const QString error_message = QString("%1  (%2)")
            .arg(QString::fromStdString(result.message))
            .arg(QString::fromStdString(ResultStatusConverter::ToString(result.status))
            );
        SetTabError(index, error_message);
        setTabText(index, QString("%1").arg(QString::fromStdString(tab.expression)));
        return;
    }
    SetTabError(index, QString());
    FillTab(tab.view, result.value);
    setTabText(index, QString::fromStdString(tab.expression));
}

// ---- change handling -----------------------------------------------------

void ExpressionDataFrameTabWidget::MarkDirty(const std::string &key)
{
    if (std::find(dirty_keys_.begin(), dirty_keys_.end(), key) == dirty_keys_.end())
    {
        dirty_keys_.push_back(key);
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

    const std::vector<std::string> keys = dirty_keys_;
    dirty_keys_.clear();

    for (const std::string &key : keys)
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
    (void)equation;
    // Tabs are all expressions and never "belong" to an equation; nothing to
    // clear pre-removal.  The post-removal re-evaluation (OnEquationRemoved)
    // turns the tab into a NameError, keeping it open watch-like.
}

void ExpressionDataFrameTabWidget::OnEquationRemoved(const std::string &equation_name)
{
    // Re-evaluate every tab that depended on the removed equation: they will
    // resolve to NameError, but the tab stays open (watch-like).
    const auto it = deps_to_keys_.find(equation_name);
    if (it == deps_to_keys_.end())
    {
        return;
    }
    for (const std::string &expression : it->second)
    {
        MarkDirty(expression);
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

    // A tab whose expression IS this equation's name is in its dependency set
    // (a self-registered dependency), so it is covered by the deps_to_keys_
    // path below.  Refresh only when the new value is ready.
    if (!(flags & EquationUpdateFlag::kValue))
    {
        return;
    }
    const auto it = deps_to_keys_.find(equation->name());
    if (it == deps_to_keys_.end())
    {
        return;
    }
    for (const std::string &expression : it->second)
    {
        MarkDirty(expression);
    }
}

void ExpressionDataFrameTabWidget::OnEquationAdded(const Equation *equation)
{
    if (!equation)
    {
        return;
    }
    // Re-evaluate tabs whose dependencies were previously missing: a tab that
    // showed NameError for a removed dep can now resolve it.
    const auto it = deps_to_keys_.find(equation->name());
    if (it == deps_to_keys_.end())
    {
        return;
    }
    for (const std::string &expression : it->second)
    {
        MarkDirty(expression);
    }
}

// ---- tabs: Add / Sync ---------------------------------------------------

void ExpressionDataFrameTabWidget::AddExpression(const std::string &expression,
                                               bool auto_pin)
{
    // Duplicate expression: focus existing tab instead of stacking a new one.
    const int existing_index = FindTabIndex(expression);
    if (existing_index >= 0)
    {
        setCurrentIndex(existing_index);
        // Re-read the value so the tab is not stale (matching SyncSelection's
        // "re-selecting refreshes" semantics).
        MarkDirty(expression);
        return;
    }

    // Parse to discover dependencies (ExpressionWatchWidget pattern) via the
    // injected callback.
    ParseResult parse_result;
    try
    {
        parse_result = manager_.Parse(expression, ParseMode::kExpression);
    }
    catch (const std::exception &)
    {
        parse_result.items.clear();
    }

    const int index = OpenTab();
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.expression = expression;
    setTabText(index, QString::fromStdString(expression));

    if (parse_result.items.size() == 1)
    {
        tab.dependencies = parse_result.items[0].dependencies;
        // Self-register: a bare equation name (or any expression that is just
        // an identifier) must also refresh when the equation's own value is
        // ready, so its kValue event reaches this tab via deps_to_keys_.
        if (std::find(tab.dependencies.begin(), tab.dependencies.end(),
                      expression) == tab.dependencies.end())
        {
            tab.dependencies.push_back(expression);
        }
        RegisterDependencies(tab.expression, tab.dependencies);
    }
    // Note: even when parsing fails, the tab stays open and shows whatever
    // EvaluateTab can produce (NameError etc.).

    RebuildKeyToIndex();
    EvaluateTab(index);
    setCurrentIndex(index);

    // New / edited expressions are auto-pinned unless the caller opts out
    // (SyncSelection passes false so selection-driven tabs keep following the
    // selection).  SetTabPinned re-orders the tab into the pinned group, and
    // the index may change.
    if (auto_pin)
    {
        const int pinned_index = FindTabIndex(expression);
        SetTabPinned(pinned_index, true);
    }
}

void ExpressionDataFrameTabWidget::SyncSelection(
    const std::vector<std::string> &selected_equation_names)
{
    // 1. Close unpinned tabs whose expression is an equation name that is no
    //    longer selected.  Compound-expression (watch) tabs and pinned tabs
    //    always survive.
    for (int i = static_cast<int>(tabs_.size()) - 1; i >= 0; --i)
    {
        TabData &tab = tabs_[static_cast<std::size_t>(i)];
        if (tab.pinned)
        {
            continue;
        }
        // Only bare identifiers can be equation-name tabs; anything with
        // operators / spaces / etc. is a plain watch expression.
        const std::string &expr = tab.expression;
        const bool is_bare_identifier =
            !expr.empty() &&
            std::all_of(expr.begin(), expr.end(), [](unsigned char c) {
                return std::isalnum(c) || c == '_';
            }) &&
            !std::isdigit(static_cast<unsigned char>(expr.front()));
        if (!is_bare_identifier)
        {
            continue;
        }
        const bool still_selected =
            std::find(selected_equation_names.begin(), selected_equation_names.end(),
                      expr) != selected_equation_names.end();
        if (still_selected)
        {
            continue;
        }
        // Only auto-close if the name was actually an equation (a plain
        // identifier that was never an equation stays open as a watch).
        if (manager_.IsEquationExist(expr))
        {
            CloseTabInternal(i);
        }
    }

    // 2. Open / refresh tabs for the selected items (AddExpression focuses /
    //    refreshes the tab each call, matching "re-selecting refreshes").
    //    Selection-driven tabs are not auto-pinned: they keep following the
    //    selection until the user pins them manually.
    for (const std::string &name : selected_equation_names)
    {
        AddExpression(name, /*auto_pin=*/false);
    }
}

void ExpressionDataFrameTabWidget::OnTabLabelDoubleClicked(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }

    TabData &tab = tabs_[static_cast<std::size_t>(index)];

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

    // Re-parse the new expression; only accept it if it parses to one item
    // (bad input leaves the tab unchanged).
    ParseResult parse_result;
    try
    {
        parse_result = manager_.Parse(trimmed, ParseMode::kExpression);
    }
    catch (const std::exception &)
    {
        parse_result.items.clear();
    }
    if (parse_result.items.size() != 1)
    {
        return;
    }

    // Swap dependencies: unregister the old key first, then adopt the new
    // expression as the tab's identity and register under it.
    UnregisterDependencies(tab.expression, tab.dependencies);
    expression_to_index_.erase(tab.expression);
    tab.expression = trimmed;
    tab.dependencies = parse_result.items[0].dependencies;
    // Self-register (same rule as AddExpression): a bare identifier must also
    // refresh when its own equation's value is ready.
    if (std::find(tab.dependencies.begin(), tab.dependencies.end(),
                  trimmed) == tab.dependencies.end())
    {
        tab.dependencies.push_back(trimmed);
    }
    RegisterDependencies(tab.expression, tab.dependencies);

    expression_to_index_[trimmed] = index;
    setTabText(index, QString::fromStdString(trimmed));
    EvaluateTab(index);

    // Edited expressions are auto-pinned like new ones (SetTabPinned re-orders
    // the tab into the pinned group; the index may change).
    const int pinned_index = FindTabIndex(trimmed);
    SetTabPinned(pinned_index, true);
}

} // namespace gui
} // namespace xresults