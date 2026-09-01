#pragma once

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
// ProxyDemoWidget -- a dual-engine (Python / REL) demo based on XEquationProxy.
//
// Features:
//   - Top: engine switch combo (Python / REL) that decides which engine the
//     following operations target.
//     + Input: name + expression (e.g. `y = [1, 2, 3]`); "Insert" inserts a
//       single Equation into the selected engine's EquationManager.
//     + "Redefine" / "Rename" / "Properties" buttons act on the selected equation.
//   - Middle-left: the current engine's Equation list (QListWidget), showing
//     only that engine's equations.
//   - Middle-right: ExpressionDataFrameView, showing the selected Equation's
//     DataFrame table (lazy loading, fetchMore). REL engine shows a DataFrame;
//     Python engine shows the object type name.
// =========================================================================

class ProxyDemoWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit ProxyDemoWidget(QWidget *parent = nullptr);
    ~ProxyDemoWidget() override;

  private:
    void SetupUI();
    void SetupConnections();

    void OnInsertEquation();
    void OnRedefineEquation();
    void OnRenameEquation();
    void OnDeleteEquation();
    void OnShowProperties();
    void OnAddWatchExpression();
    void OnEquationListSelectionChanged();
    void RefreshEquationList();

    /// Name of the currently selected list item (item text is "name  [N row(s)]").
    QString CurrentSelectedEquationName() const;

    /// Select the list item by name (used to restore selection after edit/rename).
    void SelectEquationByName(const QString &name);

    /// Parse a "name = expr" input; returns name (empty = parse failed).
    static bool SplitStatement(const QString &statement, QString *name, QString *expr);

    /// Validate the name is a legal identifier (letters/digits/underscore,
    /// not starting with a digit).
    static bool IsValidIdentifier(const QString &name);

    /// Token-level (word-boundary) replace old -> new in the expression content.
    static QString ReplaceIdentifierToken(const QString &content, const QString &old_name,
                                          const QString &new_name);

  private:
    QLineEdit *statement_edit_ = nullptr;
    QPushButton *insert_button_ = nullptr;
    QPushButton *redefine_button_ = nullptr;
    QPushButton *rename_button_ = nullptr;
    QPushButton *delete_button_ = nullptr;
    QPushButton *properties_button_ = nullptr;
    QPushButton *watch_button_ = nullptr;
    QLabel *status_label_ = nullptr;
    QListWidget *equation_list_ = nullptr;
    class ExpressionDataFrameTabWidget *data_frame_view_ = nullptr;
    class ExpressionPropertyWidget *property_widget_ = nullptr;

    /// Connections to the REL manager's kEquationRemoving / kEquationRemoved
    /// signals (auto disconnected on widget destruction); the tab widget
    /// decides which tabs to clear / re-evaluate.
    xequation::ScopedConnection removing_rel_connection_;
    xequation::ScopedConnection removed_rel_connection_;
    /// Connections to the REL manager's kEquationUpdated / kExpressionUpdated
    /// signals, for auto-refresh of tabs / property on value-ready events.
    xequation::ScopedConnection updated_rel_connection_;
    xequation::ScopedConnection expression_updated_rel_connection_;
};

} // namespace gui
} // namespace xresults
