#pragma once

#include <QTabWidget>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/equation_common.h"
#include "core/equation_manager.h"

class QMenu;
class QToolButton;

namespace xresults
{
namespace gui
{

class ExpressionDataFrameView;

// =========================================================================
// ExpressionDataFrameTabWidget -- a QTabWidget of DataFrame views, one tab per
// watched object (an Equation or a registered Expression), identified by an
// ObjectId and an explicit kind.
//
// The two kinds are opened through two distinct entries:
//   - AddEquation():  the host passes an Equation's group_id (a
//     single-equation group resolves via EquationGroup::FirstEquation); the
//     tab reads the equation value directly from the manager.
//   - AddExpression(): the host passes a registered Expression's id (obtained
//     from EquationManager::AddExpression(), which the host performs itself);
//     the manager recomputes the expression on Update()/UpdateNode() and the
//     kExpressionUpdated signal drives tab refresh.
//
// Watch expressions live in the core dependency graph; tab identity is the
// ObjectId itself, and the expression text is kept only for display / editing.
// No string-level guessing of the kind happens inside this widget: host code
// decides whether it is adding an Equation or an Expression.
//
// Data access (equation lookup / value read / expression registration /
// unregistration) is done through an EquationManager& injected by the host --
// a core-layer dependency, so this widget never touches XEquationProxy or any
// engine-specific facade.  The host passes its REL manager (DataFrames come
// only from REL).
// =========================================================================

class ExpressionDataFrameTabWidget : public QTabWidget
{
    Q_OBJECT
  public:
    explicit ExpressionDataFrameTabWidget(xequation::EquationManager &manager,
                                        QWidget *parent = nullptr);
    ~ExpressionDataFrameTabWidget() override;

    /// Open (or focus) a tab that shows an Equation's value.  The id is the
    /// Equation's group_id (single-equation group); the value is read directly
    /// from the manager -- no Eval re-evaluation.
    /// @param auto_pin  when true (default), a newly-created tab is pinned
    ///        automatically (it survives deselection); SyncSelection passes
    ///        false so selection-driven tabs keep following the selection
    ///        until the user pins them manually.
    void AddEquation(const xequation::ObjectId &group_id, bool auto_pin = true);

    /// Open (or focus) a tab that shows a registered Expression's cached
    /// value.  The id is the Expression::id returned by
    /// EquationManager::AddExpression() -- the host performs the registration
    /// and passes the id here.  The tab triggers the first synchronous
    /// computation (UpdateExpression); later refreshes are driven by the
    /// kExpressionUpdated signal.
    /// @param auto_pin  see AddEquation.
    void AddExpression(const xequation::ObjectId &expression_id, bool auto_pin = true);

    /// Register a watch expression with the manager and open a tab for it
    /// (the tab becomes an Expression tab).  No-op if the text is empty or
    /// registration fails.
    void AddWatchExpression(const std::string &expression);

    /// Close the tab at index (no-op for the last tab).
    void CloseTab(int index);

    /// Clear all tabs and unregister every watch expression (e.g. engine
    /// switch wholesale reset).
    void ClearAll();

    /// Reconcile tabs with the current list selection: tabs whose object is
    /// one of the selected equations stay / are (re)opened; unpinned
    /// equation tabs that are no longer selected are closed; pinned tabs and
    /// watch-expression tabs are always kept.  Matching tabs are re-read
    /// (refreshed) on every sync, so re-selecting refreshes them.
    void SyncSelection(const std::vector<std::string> &selected_equation_names);

    // ---- change routing (external code connects the engine) -----------

    void OnEquationRemoving(const xequation::Equation *equation);
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);
    void OnExpressionUpdated(const xequation::Expression *expression,
                             bitmask::bitmask<xequation::ExpressionUpdateFlag> flags);

  private:
    /// What an ObjectId in a tab refers to.
    enum class ObjectKind
    {
        kEquation,
        kExpression,
    };

    /// A tab's identity + source descriptor.
    struct TabData
    {
        ObjectKind kind = ObjectKind::kEquation;
        xequation::ObjectId object_id;       // equation group_id / expression id
        std::string expression;              // display text + edit source
        ExpressionDataFrameView *view = nullptr;
        bool pinned = false;                 // pinned tabs survive deselection
        QToolButton *pin_button = nullptr;
        QToolButton *close_button = nullptr;
    };

    // ---- tab lifecycle ----
    int FindTabIndex(const xequation::ObjectId &object_id) const;
    int OpenTab();
    void CloseTabInternal(int index);
    void FillTab(ExpressionDataFrameView *view, const xequation::EquationValue &value);

    // ---- evaluation ----
    void EvaluateTab(int index);

    // ---- pinning / ordering ----
    int IndexOfPinButton(const QToolButton *pin) const;
    void MoveTab(int from, int to);
    void RebuildKeyToIndex();
    void UpdatePinButton(int index);
    /// Set the tab's pinned state, update the pin button, and re-order the
    /// tab into the pinned / unpinned group.  Shared by the pin button and the
    /// auto-pin behavior (new / edited expressions are pinned by default).
    void SetTabPinned(int index, bool pinned);
    /// Show / clear the error overlay on the tab's table view.
    void SetTabError(int index, const QString &message);

    // ---- label editing ----
    void OnTabLabelDoubleClicked(int index);
    void OnTabContextMenu(const QPoint &pos);
    /// Edit a tab's text; an Equation tab becomes a watch Expression (its
    /// equation is kept as-is).
    void EditTab(int index);

    /// Show / hide an unpinned pin button on hover (pinned buttons stay
    /// visible).  Installed as an event filter on each pin button.
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    /// REL manager used for resolve / register / unregister (host-provided;
    /// must outlive this widget -- the proxy singleton does).
    xequation::EquationManager &manager_;
    std::vector<TabData> tabs_;
    std::unordered_map<xequation::ObjectId, int> object_to_index_;
};

} // namespace gui
} // namespace xresults