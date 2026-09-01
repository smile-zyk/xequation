#pragma once

#include <QTabWidget>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_manager.h"

class QTimer;
class QToolButton;

namespace xresults
{
namespace gui
{

class ExpressionDataFrameView;

// =========================================================================
// ExpressionDataFrameTabWidget -- a QTabWidget of DataFrame views, one tab per
// watched expression (modeled on ExpressionWatchWidget's dependency tracking).
//
// Data access (parse / eval / equation lookup) is done through a const
// EquationManager& injected by the host -- a core-layer dependency, so this
// widget never touches XEquationProxy or any engine-specific facade.  The
// host passes its REL manager (DataFrames come only from REL).
// =========================================================================

class ExpressionDataFrameTabWidget : public QTabWidget
{
    Q_OBJECT
  public:
    explicit ExpressionDataFrameTabWidget(const xequation::EquationManager &manager,
                                        QWidget *parent = nullptr);
    ~ExpressionDataFrameTabWidget() override;

    /// Open (or focus) a tab that watches an expression.  Everything is an
    /// expression: passing an equation's name shows its value (read directly
    /// from the REL manager, no Eval re-evaluation); passing a compound
    /// expression evaluates it through the REL engine whenever its
    /// dependencies change.
    /// @param auto_pin  when true (default), a newly-created tab is pinned
    ///        automatically (it survives deselection); SyncSelection passes
    ///        false so selection-driven tabs keep following the selection
    ///        until the user pins them manually.
    void AddExpression(const std::string &expression, bool auto_pin = true);

    /// Close the tab at index (no-op for the last tab).
    void CloseTab(int index);

    /// Clear all tabs (e.g. engine switch wholesale reset).
    void ClearAll();

    /// Reconcile tabs with the current list selection: tabs whose expression
    /// is one of the selected equation names stay / are (re)opened; unpinned
    /// equation-name tabs that are no longer selected are closed; pinned tabs
    /// and compound-expression (watch) tabs are always kept.  Matching tabs are
    /// re-read (refreshed) on every sync, so re-selecting refreshes them.
    void SyncSelection(const std::vector<std::string> &selected_equation_names);

    // ---- change routing (external code connects both engines) ----------

    void OnEquationRemoving(const xequation::Equation *equation);
    void OnEquationRemoved(const std::string &equation_name);
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);
    void OnEquationAdded(const xequation::Equation *equation);

  private:
    /// A tab's source descriptor + dependency set (for kValue-driven refresh).
    struct TabData
    {
        std::string expression;          // identity + display text + re-eval text
        std::vector<std::string> dependencies;  // parse-time dep names (+ self)
        ExpressionDataFrameView *view = nullptr;
        bool pinned = false;             // pinned tabs survive deselection
        QToolButton *pin_button = nullptr;
    };

    // ---- tab lifecycle ----
    int FindTabIndex(const std::string &expression) const;
    int OpenTab();
    void CloseTabInternal(int index);
    void FillTab(ExpressionDataFrameView *view, const xequation::EquationValue &value);

    // ---- evaluation ----
    void EvaluateTab(int index);

    // ---- change handling ----
    void MarkDirty(const std::string &expression);
    void ScheduleReeval();
    void OnReevalTimer();

    // ---- pinning / ordering ----
    void MoveTab(int from, int to);
    void RebuildKeyToIndex();
    void UpdatePinButton(int index);
    void OnPinButtonClicked(int index, bool checked);
    /// Set the tab's pinned state, update the pin button, and re-order the
    /// tab into the pinned / unpinned group.  Shared by the pin button and the
    /// auto-pin behavior (new / edited expressions are pinned by default).
    void SetTabPinned(int index, bool pinned);
    /// Show / clear the error overlay on the tab's table view.
    void SetTabError(int index, const QString &message);

    // ---- dependency registration ----
    void UnregisterDependencies(const std::string &expression,
                                const std::vector<std::string> &deps);
    void RegisterDependencies(const std::string &expression,
                              const std::vector<std::string> &deps);

    // ---- label editing ----
    void OnTabLabelDoubleClicked(int index);

  private:
    /// REL manager used for parse / eval / equation lookup (host-provided;
    /// must outlive this widget -- the proxy singleton does).
    const xequation::EquationManager &manager_;
    std::vector<TabData> tabs_;
    std::unordered_map<std::string, int> expression_to_index_;
    /// equation name -> tab expressions that depend on it (bimap, watch-like)
    std::unordered_map<std::string, std::vector<std::string>> deps_to_keys_;
    /// keys marked dirty by a change event; coalesced re-eval
    std::vector<std::string> dirty_keys_;
    bool reeval_scheduled_ = false;
    QTimer *reeval_timer_ = nullptr;
};

} // namespace gui
} // namespace xresults