#pragma once

#include <QString>
#include <QWidget>

#include "core/equation_signals_manager.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QLabel;

namespace xresults
{
namespace gui
{

// =========================================================================
// DemoWidget -- a REL-engine demo built on EquationManager::GetInstance().
//
// Features:
//   + Input: name + expression (e.g. `y = [1, 2, 3]`); "Insert" inserts a
//     single Equation into the engine's EquationManager.
//   + "Redefine" / "Rename" buttons act on the selected equation.
//   - Middle-left: the engine's Equation list (QListWidget).
//   - Middle-right: ExpressionDataFrameView, showing the selected Equation's
//     DataFrame table (lazy loading, fetchMore).
// =========================================================================

class DemoWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit DemoWidget(QWidget *parent = nullptr);
    ~DemoWidget() override;

  private:
    void SetupUI();
    void SetupConnections();

    void OnInsertEquation();
    void OnRedefineEquation();
    void OnRenameEquation();
    void OnDeleteEquation();
    void OnAddWatchExpression();
    void OnEquationListSelectionChanged();
    void RefreshEquationList();

    // ---- equation-manager tree panel ----------------------------------

    /// User selected a node in the manager tree (dataset / block / data array
    /// / equation / expression).  Routes the payload to the property widget
    /// and, for data arrays / equations / expressions, to the DataFrame tab.
    void OnManagerTreeSelectionChanged();
    void OnManagerTreeClicked();

    // ---- dataset (env.json) support -----------------------------------

    /// Open a file dialog for an environment JSON and load its datasets.
    void OnOpenEnvJson();

    /// User picked another dataset in the combo -> make it the REL default
    /// dataset and recompute everything (bare DataArray names resolve against
    /// the default dataset).
    void OnDatasetSelectionChanged(int index);

    /// Rebuild the combo contents from rel::Environment::DatasetNames() and
    /// select the REL default dataset.
    void RefreshDatasetCombo();

    /// Loads the datasets of an env.json into the REL environment.  Reuses
    /// rel::Environment::LoadFromConfig (dataset files + default dataset +
    /// python_plugins); datasets of a previously loaded env are dropped first
    /// so re-opening replaces the active set.  Also refreshes the combo and
    /// re-evaluates all equations.  Shows a warning box on failure.
    void LoadEnvJson(const QString &path);

    /// Name of the currently selected list item (item text is "name  [N row(s)]").
    QString CurrentSelectedEquationName() const;

    /// Select the list item by name (used to restore selection after edit/rename).
    void SelectEquationByName(const QString &name);

    /// Parse a "name = expr" input; returns name (empty = parse failed).
    static bool SplitStatement(const QString &statement, QString *name, QString *expr);

    /// Validate the name is a legal identifier (letters/digits/underscore,
    /// not starting with a digit).
    static bool IsValidIdentifier(const QString &name);

  private:
    QLineEdit *statement_edit_ = nullptr;
    QPushButton *insert_button_ = nullptr;
    QPushButton *redefine_button_ = nullptr;
    QPushButton *rename_button_ = nullptr;
    QPushButton *delete_button_ = nullptr;
    QPushButton *watch_button_ = nullptr;
    QPushButton *open_env_button_ = nullptr;
    QComboBox *dataset_combo_ = nullptr;
    QLabel *status_label_ = nullptr;
    QListWidget *equation_list_ = nullptr;
    class EquationManagerTreeView *manager_tree_ = nullptr;
    class ExpressionDataFrameTabWidget *data_frame_view_ = nullptr;
    class ExpressionPropertyWidget *property_widget_ = nullptr;

    /// Absolute path of the last successfully loaded env.json (empty = none).
    QString env_json_path_;

    /// Connections to the REL manager's kEquationRemoving / kEquationUpdated
    /// signals (auto disconnected on widget destruction); the tab widget
    /// decides which tabs to clear / refresh.
    xequation::ScopedConnection removing_rel_connection_;
    xequation::ScopedConnection updated_rel_connection_;

    /// Connection to the REL manager's kEquationRemoved signal: an equation
    /// left the manager (Delete button, or the manager-tree context menu
    /// which calls RemoveEquation directly) -- refresh the equation list so
    /// the middle-left panel stays in sync.
    xequation::ScopedConnection equation_removed_rel_connection_;

    /// Connection to the REL manager's kExpressionUpdated signal, for
    /// auto-refresh of watch-expression tabs / property on value-ready.
    xequation::ScopedConnection expression_updated_rel_connection_;
    /// Connection to the REL manager's kExpressionRemoving signal: the
    /// expression left the manager (tree-leaf Delete / env reload); close its
    /// tab and clear the property widget.
    xequation::ScopedConnection expression_removing_rel_connection_;
};

} // namespace gui
} // namespace xresults
