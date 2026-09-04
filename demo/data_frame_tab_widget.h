#pragma once

#include <QString>
#include <QTabWidget>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/equation_common.h"
#include "core/equation_manager.h"

class QMenu;
class QAction;
class QToolButton;

namespace xdataset
{
class Block;
}

namespace xresults
{
namespace gui
{

class DataFrameView;

// =========================================================================
// DataFrameTabWidget -- a QTabWidget of DataFrame views, one tab per
// watched object (an Equation or a registered Expression), identified by an
// ObjectId and an explicit kind (kEquation / kExpression -- the kind is only
// a display/eval hint; the object identity is the ObjectId).
//
// The two kinds are opened through two distinct entries:
//   - AddEquation():  the host passes an Equation's id; the tab reads the
//     equation value directly from the manager.
//   - AddExpression(): the host passes a registered Expression's id (obtained
//     from EquationManager::AddExpression(), which the host performs itself);
//     the manager recomputes the expression on Update()/UpdateNode() and the
//     kExpressionUpdated signal drives tab refresh.
//
// Tab content is purely a VIEW of a manager object: closing a tab never
// unregisters the object.  Expression lifetime is owned by the manager-tree
// item that created it (see ExplorerView); when an expression is
// removed from the manager (tree right-click Delete / env reload), the host
// routes kExpressionRemoving here and the tab closes via OnExpressionRemoving.
//
// Data access (equation lookup / value read / expression registration /
// unregistration) is done through an EquationManager& injected by the host --
// a core-layer dependency, so this widget never touches the engine facade or any
// engine-specific facade.  The host passes its REL manager (DataFrames come
// only from REL).
// =========================================================================

class DataFrameTabWidget : public QTabWidget
{
    Q_OBJECT
  public:
    explicit DataFrameTabWidget(xequation::EquationManager &manager,
                                QWidget *parent = nullptr);
    ~DataFrameTabWidget() override;

    /// Open (or focus) a tab that shows an Equation's value.  The id is the
    /// Equation's id; the value is read directly from the manager -- no Eval
    /// re-evaluation.
    /// @param auto_pin  when true (default), a newly-created tab is pinned
    ///        automatically (it survives deselection); SyncTabs passes false
    ///        so selection-driven tabs keep following the selection until the
    ///        user pins them manually.
    void AddEquation(const xequation::ObjectId &equation_id, bool auto_pin = true);

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

    /// Close the tab at index (view only -- the underlying object, if it is a
    /// registered expression, stays registered in the manager).
    void CloseTab(int index);

    /// Close every tab (view only; no expression is unregistered here).
    void ClearAll();

    /// Reconcile tabs with the visible ObjectId set (the current selection of
    /// the last-clicked panel, list or tree):
    ///   - pinned tabs are always kept;
    ///   - unpinned tabs whose object is not in @p visible_ids are closed;
    ///   - objects in @p visible_ids without an open tab are opened (equation
    ///     or expression is resolved through the manager), not auto-pinned.
    /// Matching tabs are re-read (refreshed) on every sync, so re-selecting
    /// refreshes them.
    void SyncTabs(const std::vector<xequation::ObjectId> &visible_ids);

    /// Reconcile the block tabs with the visible Block set (the current
    /// selection of the manager tree).  A Block has no ObjectId -- it is
    /// identified by (dataset, block_path) -- so its tabs live in a separate
    /// map and follow the same rules as SyncTabs: pinned block tabs survive,
    /// unpinned Block tabs close when the Block is no longer selected, and a
    /// newly-selected Block opens a tab.  Call this BEFORE SyncTabs so an
    /// ObjectId-tab mirror does not drop the block tabs.
    void SyncBlockTabs(const std::vector<std::pair<QString, QString>> &visible_blocks);

    /// Close every block tab, even pinned ones.  Called on an environment
    /// reload (datasets/blocks are dropped): a Block tab's frame is owned and
    /// cached by the Block, so once the Block is destroyed the frame dangles.
    void ClearBlockTabs();

    // ---- change routing (external code connects the engine) -----------

    void OnEquationRemoving(const xequation::Equation *equation);
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);
    /// The expression is gone from the manager (tree Delete / env reload):
    /// close its tab, even when pinned.
    void OnExpressionRemoving(const xequation::Expression *expression);
    void OnExpressionUpdated(const xequation::Expression *expression,
                             bitmask::bitmask<xequation::ExpressionUpdateFlag> flags);

  private:
    /// What an ObjectId in a tab refers to.
    enum class ObjectKind
    {
        kEquation,
        kExpression,
        kBlock,   // tab shows a Block's DataFrame (no ObjectId)
    };

    /// A tab's identity + source descriptor.
    struct TabData
    {
        ObjectKind kind = ObjectKind::kEquation;
        xequation::ObjectId object_id = xequation::NilObjectId();  // equation/expression id
        /// For a Block tab (ObjectKind::kBlock), the (dataset, block_path)
        /// identity used to key block_to_index_; empty otherwise.
        QString block_dataset;
        QString block_path;
        std::string expression;              // display text + edit source
        DataFrameView *view = nullptr;
        bool pinned = false;                 // pinned tabs survive deselection
        QToolButton *pin_button = nullptr;
        QToolButton *close_button = nullptr;
    };

    /// Open (or focus) a tab that shows a Block's tabulated frame.  A Block
    /// has no ObjectId, so the identity is (dataset, block_path); the tab
    /// resolves the Block through the REL environment and displays its frame
    /// directly.  Duplicate focus re-reads the tab.  Not auto-pinned unless
    /// requested.
    void AddBlockTab(const QString &dataset, const QString &block_path,
                     bool auto_pin = false);
    /// Find the tab index owning (dataset, block_path); -1 if none.
    int FindBlockTabIndex(const QString &dataset, const QString &block_path) const;

    // ---- tab lifecycle ----
    int FindTabIndex(const xequation::ObjectId &object_id) const;
    int OpenTab();
    void CloseTabInternal(int index);
    void FillTab(DataFrameView *view, const xequation::EquationValue &value);

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
    /// Show the shared context menu for the tab at the given tab-bar position.
    /// One menu (built once) is reused for every tab; the Edit / Delete
    /// actions are enabled only for the applicable tab.
    void OnTabContextMenu(const QPoint &pos);
    /// Whether the tab may be edited.  Only "Watch"-tagged expressions are
    /// user-editable: Equation, Block, and DataArray-access (and any other
    /// internal / host-only tagged) expressions are read-only.
    bool IsTabEditable(int index) const;
    /// Edit a tab's text; only a "Watch"-tagged expression tab is editable.
    /// Editing turns the tab into a re-registered watch Expression (its
    /// original object, if any, is kept as-is / released as appropriate).
    void EditTab(int index);

    /// Show / hide an unpinned pin button on hover (pinned buttons stay
    /// visible).  Installed as an event filter on each pin button.
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    /// REL manager used for resolve / register / unregister (host-provided;
    /// must outlive this widget -- the proxy singleton does).
    xequation::EquationManager &manager_;

    /// The tab index the menu was opened for (the right-clicked tab).  The
    /// Edit / Delete actions act on this precise tab.
    int context_target_index_ = -1;

    // Shared context menu: built once, shown for every tab.  The Edit / Delete
    // actions' enabled state is updated per tab before showing.
    QMenu *context_menu_ = nullptr;
    QAction *edit_action_ = nullptr;
    QAction *delete_action_ = nullptr;
    QAction *add_watch_action_ = nullptr;

    std::vector<TabData> tabs_;
    std::unordered_map<xequation::ObjectId, int> object_to_index_;
    /// Block tabs are keyed by (dataset, block_path) -- a Block has no
    /// ObjectId, so its tabs are NOT in object_to_index_.  Maps to a tab index
    /// in tabs_.
    std::map<std::pair<QString, QString>, int> block_to_index_;
};

} // namespace gui
} // namespace xresults