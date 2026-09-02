#include "expression_data_frame_tab_widget.h"

#include "expression_data_frame_view.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QTabBar>
#include <QToolButton>

#include <algorithm>

namespace xresults
{
namespace gui
{

using namespace xequation;

namespace
{
/// Transparent 16x16 icon: used for the unpinned pin button while it is not
/// hovered -- the button keeps its slot in the layout (no width collapse)
/// but draws nothing.
QIcon MakeBlankIcon()
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    return QIcon(pm);
}

/// Horizontal (lying-down) pushpin: cap on the LEFT, needle pointing RIGHT.
/// Used while the tab is unpinned (shown only on hover, like VS).
QIcon MakePinIconHorizontal(const QPalette &pal)
{
    const QColor fg = pal.color(QPalette::Foreground);
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(fg, 1.5));
    painter.setBrush(Qt::NoBrush);

    // Cap: rounded rect on the left.
    painter.drawRoundedRect(QRectF(2.0, 3.6, 6.6, 8.8), 1.8, 1.8);
    // Shoulder: vertical line just right of the cap.
    painter.drawLine(QPointF(9.5, 2.8), QPointF(9.5, 13.2));
    // Needle: horizontal from the shoulder to the right.
    painter.drawLine(QPointF(9.5, 8.0), QPointF(14.0, 8.0));

    return QIcon(pm);
}

/// Vertical (upright) pushpin: cap on TOP, needle pointing DOWN.
/// Used while the tab is pinned (always shown).
QIcon MakePinIconVertical(const QPalette &pal)
{
    const QColor fg = pal.color(QPalette::Foreground);
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(fg, 1.5));
    painter.setBrush(QBrush(fg));   // pinned = solid cap

    // Cap: rounded rect on top.
    painter.drawRoundedRect(QRectF(3.6, 2.0, 8.8, 6.6), 1.8, 1.8);
    // Shoulder: horizontal line just below the cap.
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(2.8, 9.5), QPointF(13.2, 9.5));
    // Needle: vertical from the shoulder down to the bottom.
    painter.drawLine(QPointF(8.0, 9.5), QPointF(8.0, 14.0));

    return QIcon(pm);
}

/// 16x16 icon: an "X" cross (same weight as the pin circle).
QIcon MakeCloseIcon(const QPalette &pal)
{
    const QColor fg = pal.color(QPalette::Foreground);
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(fg, 1.4));
    painter.drawLine(QPointF(4.5, 4.5), QPointF(11.5, 11.5));
    painter.drawLine(QPointF(11.5, 4.5), QPointF(4.5, 11.5));
    return QIcon(pm);
}

/// Tab-bar button shared look: small, flat, no focus rect, hand cursor.
QToolButton *MakeTabBarButton(QWidget *parent, const QIcon &icon, const QString &tooltip)
{
    auto *button = new QToolButton(parent);
    button->setIcon(icon);
    button->setIconSize(QSize(16, 16));
    button->setFixedSize(QSize(16, 16));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    return button;
}
} // namespace

// =========================================================================
// ExpressionDataFrameTabWidget
// =========================================================================

ExpressionDataFrameTabWidget::ExpressionDataFrameTabWidget(
    EquationManager &manager, QWidget *parent)
    : QTabWidget(parent), manager_(manager)
{
    // Built-in close buttons are disabled: pin + close are both custom and
    // share the same look (see OpenTab), packed [pin][close] on the right.
    setTabsClosable(false);
    setDocumentMode(true);
    connect(tabBar(), &QTabBar::tabBarDoubleClicked,
            this, &ExpressionDataFrameTabWidget::OnTabLabelDoubleClicked);
    // Right-click on a tab: Edit / Delete / Add Watch Expression.
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested,
            this, &ExpressionDataFrameTabWidget::OnTabContextMenu);
}

ExpressionDataFrameTabWidget::~ExpressionDataFrameTabWidget()
{
    // Unregister every remaining registered expression (equation tabs hold an
    // equation id; RemoveExpression is a no-op for them).  The view widgets are
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
    TabData &tab = tabs_.back();

    // ------------------------------------------------------------------
    // Tab bar buttons: pin + close share the same look and are packed on the
    // right side of the tab with pin on the LEFT of close:  [pin][close].
    // ------------------------------------------------------------------
    auto *container = new QWidget(tabBar());
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Not checkable: the pin on/off is shown by the icon direction (lying =
    // unpinned, upright = pinned).  autoRaise gives hover + press feedback.
    auto *pin_button = MakeTabBarButton(
        container, MakeBlankIcon(),
        QStringLiteral("Pin this tab: keep it when deselected")
    );
    auto *close_button = MakeTabBarButton(
        container, MakeCloseIcon(tabBar()->palette()),
        QStringLiteral("Close this tab")
    );

    tab.pin_button = pin_button;
    tab.close_button = close_button;

    layout->addWidget(pin_button);
    layout->addWidget(close_button);

    tabBar()->setTabButton(index, QTabBar::RightSide, container);

    // The pin button must be visible (blank icon) to receive Enter/Leave.
    pin_button->installEventFilter(this);

    // Resolve the tab index by button identity at click time: tabs may have
    // been re-ordered by pin/unpin since the button was created.  The button
    // is not checkable; a click toggles the tab's pinned state.
    connect(pin_button, &QToolButton::clicked, this, [this, pin_button]() {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
        {
            if (tabs_[static_cast<std::size_t>(i)].pin_button == pin_button)
            {
                TabData &t = tabs_[static_cast<std::size_t>(i)];
                SetTabPinned(i, !t.pinned);
                return;
            }
        }
    });
    connect(close_button, &QToolButton::clicked, this, [this, close_button]() {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
        {
            if (tabs_[static_cast<std::size_t>(i)].close_button == close_button)
            {
                CloseTab(i);
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
    object_to_index_.erase(tab.object_id);

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
}

void ExpressionDataFrameTabWidget::FillTab(ExpressionDataFrameView *view,
                                         const EquationValue &value)
{
    view->SetValue(value);
}

// ---- pinning / ordering ---------------------------------------------------

/// Find the tab index owning the given pin button (by identity); -1 if none.
int ExpressionDataFrameTabWidget::IndexOfPinButton(const QToolButton *pin) const
{
    for (std::size_t i = 0; i < tabs_.size(); ++i)
    {
        if (tabs_[i].pin_button == pin)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

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
    // Pinned -> upright solid pin (always drawn); unpinned -> blank (the
    // hover overlay draws the lying pin).  The button is not checkable, so
    // there is no sunken checked look -- state is conveyed by the icon.
    tab.pin_button->setIcon(checked ? MakePinIconVertical(tabBar()->palette())
                                    : MakeBlankIcon());
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

bool ExpressionDataFrameTabWidget::eventFilter(QObject *obj, QEvent *event)
{
    // Pin buttons are visible (empty icon) and installed with this filter:
    // on Enter, draw the lying pin (if unpinned); on Leave, blank it back.
    auto *pin = qobject_cast<QToolButton *>(obj);
    if (pin)
    {
        // Find the owning tab by button identity.
        const int index = IndexOfPinButton(pin);
        if (index >= 0)
        {
            TabData &tab = tabs_[static_cast<std::size_t>(index)];
            if (event->type() == QEvent::Enter)
            {
                if (!tab.pinned)
                {
                    tab.pin_button->setIcon(MakePinIconHorizontal(tabBar()->palette()));
                }
                return false;
            }
            if (event->type() == QEvent::Leave)
            {
                if (!tab.pinned)
                {
                    tab.pin_button->setIcon(MakeBlankIcon());
                }
                return false;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
// ---- evaluation ----------------------------------------------------------

void ExpressionDataFrameTabWidget::EvaluateTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }
    const TabData &tab = tabs_[static_cast<std::size_t>(index)];

    // Raw value tabs are manager-independent: they were filled by ShowValue()
    // and have nothing to evaluate against the manager.
    if (tab.kind == ObjectKind::kValue)
    {
        return;
    }

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

    // Equation tab: read the value directly by id.
    const Equation *equation = manager_.GetEquationById(tab.object_id);
    if (equation && equation->status == ResultStatus::kSuccess)
    {
        SetTabError(index, QString());
        FillTab(tab.view, manager_.GetEquationValue(equation->name));
        setTabText(index, QString::fromStdString(tab.expression));
        return;
    }
    tab.view->Clear();
    const QString error_message =
        equation ? QString("%1  (%2)")
                       .arg(QString::fromStdString(equation->message))
                       .arg(QString::fromStdString(ResultStatusConverter::ToString(equation->status)))
                 : QStringLiteral("Equation does not exist.");
    SetTabError(index, error_message);
    setTabText(index, QString::fromStdString(tab.expression));
}

// ---- change routing ------------------------------------------------------

void ExpressionDataFrameTabWidget::OnEquationRemoving(const Equation *equation)
{
    if (!equation)
    {
        return;
    }
    // kEquationRemoving is emitted *before* the equation is erased from the
    // manager, so a synchronous EvaluateTab here would re-read the old value.
    // The equation is going away: close its tab entirely.  (CloseTabInternal
    // calls RemoveExpression on the group id, which is a no-op -- it only
    // releases registered expressions.)  Watch-expression tabs are untouched;
    // the removal recomputation arrives via kExpressionUpdated(kValue).
    const int index = FindTabIndex(equation->id);
    if (index >= 0)
    {
        CloseTabInternal(index);
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

    // Equation tabs whose id matches this equation are refreshed.
    // Watch-expression tabs are NOT touched here: the manager recomputes
    // dependent expressions during the same update pass and their own
    // kExpressionUpdated(kValue) event refreshes them.
    const int index = FindTabIndex(equation->id);
    if (index >= 0)
    {
        EvaluateTab(index);
    }
}

void ExpressionDataFrameTabWidget::OnExpressionUpdated(
    const Expression *expression, bitmask::bitmask<ExpressionUpdateFlag> flags
)
{
    if (!expression)
    {
        return;
    }
    // Refresh only when the new value is ready: the manager emits twice per
    // computation (a "Calculating..." kStatus|kMessage event first, then
    // kStatus|kMessage|kValue), so filtering on kValue drops the intermediate
    // state and avoids re-filling the tab with a stale/"Calculating" cache.
    if (!(flags & ExpressionUpdateFlag::kValue))
    {
        return;
    }
    // Watch-expression tab: refresh from the (already recomputed) cached value.
    const int index = FindTabIndex(expression->id);
    if (index >= 0)
    {
        EvaluateTab(index);
    }
}

// ---- tabs: Add -----------------------------------------------------------

void ExpressionDataFrameTabWidget::AddEquation(const ObjectId &equation_id, bool auto_pin)
{
    if (equation_id.is_nil())
    {
        return;
    }

    // Duplicate object: focus existing tab instead of stacking a new one.
    const int existing_index = FindTabIndex(equation_id);
    if (existing_index >= 0)
    {
        setCurrentIndex(existing_index);
        // Re-read the value so the tab is not stale (matching SyncSelection's
        // "re-selecting refreshes" semantics).
        EvaluateTab(existing_index);
        return;
    }

    const Equation *equation = manager_.GetEquationById(equation_id);
    if (!equation)
    {
        return;
    }

    const int index = OpenTab();
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.kind = ObjectKind::kEquation;
    tab.object_id = equation_id;
    tab.expression = equation->name;
    setTabText(index, QString::fromStdString(tab.expression));

    RebuildKeyToIndex();
    EvaluateTab(index);
    setCurrentIndex(index);

    if (auto_pin)
    {
        const int pinned_index = FindTabIndex(equation_id);
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
        EvaluateTab(existing_index);
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
    setTabText(index, QString::fromStdString(tab.expression));

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

void ExpressionDataFrameTabWidget::ShowValue(const QString &title,
                                             const EquationValue &value)
{
    // A single "raw value" preview tab: find it by kind (not by ObjectId,
    // which is nil for raw values) and update it in place.  It is NOT
    // auto-pinned: showing a value is selection-driven display, not an
    // explicit watch, so the tab follows the current selection.
    int index = -1;
    for (std::size_t i = 0; i < tabs_.size(); ++i)
    {
        if (tabs_[i].kind == ObjectKind::kValue)
        {
            index = static_cast<int>(i);
            break;
        }
    }

    if (index < 0)
    {
        index = OpenTab();
        TabData &tab = tabs_[static_cast<std::size_t>(index)];
        tab.kind = ObjectKind::kValue;
        tab.object_id = ObjectId();   // not manager-bound
        RebuildKeyToIndex();
    }

    if (index < 0)
    {
        return;
    }
    TabData &tab = tabs_[static_cast<std::size_t>(index)];
    tab.expression = title.toStdString();
    setTabText(index, title);
    FillTab(tab.view, value);
    setCurrentIndex(index);
}

void ExpressionDataFrameTabWidget::SyncSelection(
    const std::vector<std::string> &selected_equation_names)
{
    // Sync: keep ONLY pinned tabs and tabs whose equation is currently
    // selected.  Everything else (unpinned equation tabs AND unpinned
    // expression/watch tabs) is closed -- that is what makes pinning useful.
    for (int i = static_cast<int>(tabs_.size()) - 1; i >= 0; --i)
    {
        TabData &tab = tabs_[static_cast<std::size_t>(i)];
        if (tab.pinned)
        {
            continue;
        }
        // Is this tab's expression one of the selected equation names?
        const bool still_selected =
            std::find(selected_equation_names.begin(), selected_equation_names.end(),
                      tab.expression) != selected_equation_names.end();
        if (still_selected)
        {
            continue;
        }
        CloseTabInternal(i);
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
            AddEquation(equation->id, /*auto_pin=*/false);
        }
    }
}

void ExpressionDataFrameTabWidget::OnTabLabelDoubleClicked(int index)
{
    EditTab(index);
}

void ExpressionDataFrameTabWidget::EditTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size()))
    {
        return;
    }

    TabData &tab = tabs_[static_cast<std::size_t>(index)];

    // Raw value tabs are not editable (nothing to register in the manager).
    if (tab.kind == ObjectKind::kValue)
    {
        return;
    }

    // Both Equation and Expression tabs are editable.  Editing an Equation
    // tab turns it into a registered watch Expression (the equation itself is
    // left untouched in the manager).
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

    // Register the new expression first; only on success release the old one,
    // so a failed parse leaves the tab (and its old object) intact.
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
        return;   // parse failed: tab unchanged
    }

    // Release the previous registered expression (no-op for an equation id).
    // Re-key the tab to the new expression.
    if (tab.kind == ObjectKind::kExpression)
    {
        manager_.RemoveExpression(tab.object_id);
    }
    object_to_index_.erase(tab.object_id);

    tab.kind = ObjectKind::kExpression;
    tab.object_id = new_id;
    tab.expression = trimmed;

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

void ExpressionDataFrameTabWidget::OnTabContextMenu(const QPoint &pos)
{
    const int index = tabBar()->tabAt(pos);
    if (index < 0)
    {
        return;
    }

    QMenu menu(this);
    QAction *edit_action = menu.addAction(QStringLiteral("Edit"));
    QAction *delete_action = menu.addAction(QStringLiteral("Delete"));
    menu.addSeparator();
    QAction *add_watch_action = menu.addAction(QStringLiteral("Add Watch Expression"));

    QAction *chosen = menu.exec(tabBar()->mapToGlobal(pos));
    if (chosen == edit_action)
    {
        EditTab(index);
    }
    else if (chosen == delete_action)
    {
        CloseTab(index);
    }
    else if (chosen == add_watch_action)
    {
        bool ok = false;
        const QString expression = QInputDialog::getText(
            this, QStringLiteral("Add Watch Expression"),
            QStringLiteral("Expression:"), QLineEdit::Normal, QString(), &ok);
        if (ok)
        {
            AddWatchExpression(expression.trimmed().toStdString());
        }
    }
}

void ExpressionDataFrameTabWidget::AddWatchExpression(const std::string &expression)
{
    if (expression.empty())
    {
        return;
    }
    ObjectId id;
    try
    {
        id = manager_.AddExpression(expression);
    }
    catch (const std::exception &)
    {
        id = ObjectId();
    }
    if (!id.is_nil())
    {
        AddExpression(id);
    }
}

} // namespace gui
} // namespace xresults