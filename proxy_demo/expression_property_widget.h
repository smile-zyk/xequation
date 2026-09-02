#pragma once

#include <QWidget>

#include "core/equation_manager.h"

class QTableWidget;
class QTableWidgetItem;

namespace xresults
{
namespace gui
{

// =========================================================================
// ExpressionPropertyWidget -- property display widget for a single Equation
// or registered Expression, identified by an ObjectId (resident; refreshable).
//
// An ObjectId denotes one object in the manager:
//   - a registered Expression (its Expression::id), or
//   - an Equation (its Equation::id).
//
// Shows the properties in a read-only two-column table (field | value):
//   1. Equation meta info (mirrors EquationBrowser): Name / Expression / Type /
//      Status / Message / Dependencies / Dependents; for an Expression: ID /
//      Expression / Status / Message / Dependencies.
//   2. Engine value info:
//      - REL engine: mirrors builtin_library's what(x) output -- Indep / Kind /
//        Dimension / Data Shape / Data Type / Unit, then the Value on demand.
//
// The table is non-editable (NoEditTriggers) and display-only.  Refresh via
// SetObject() when the selected object changes; external code connects the
// manager's kEquationRemoving / kEquationUpdated / kExpressionUpdated
// signals to the slots below; this widget decides whether it displays that
// object.
// =========================================================================

class ExpressionPropertyWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit ExpressionPropertyWidget(const xequation::EquationManager &manager,
                                      QWidget *parent = nullptr);
    ~ExpressionPropertyWidget() override;

    /// Set the object to display (refresh).  A default-constructed (nil) id
    /// clears the table and shows a placeholder.
    void SetObject(const xequation::ObjectId &object_id);

    /// External code connects both engines' managers' kEquationRemoving signal
    /// here (fired before deletion).  If the removed equation is the one being
    /// displayed, clears.
    void OnEquationRemoving(const xequation::Equation *equation);

    /// External code connects both engines' managers' kEquationUpdated signal
    /// here (fired on equation update).  If the updated equation is the one
    /// being displayed, reloads.
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);

    /// External code connects both engines' managers' kExpressionUpdated signal
    /// here (fired on registered-expression update).  If the updated expression
    /// is the one being displayed, reloads.
    void OnExpressionUpdated(const xequation::Expression *expression,
                             bitmask::bitmask<xequation::ExpressionUpdateFlag> flags);

    /// Display arbitrary (field | value) rows that are not bound to a manager
    /// ObjectId (e.g. Dataset / Block inspection).  The heading is rendered as
    /// the first field row.  Clears any object binding so manager refresh
    /// signals do not clobber this view.
    void ShowInfo(const QString &heading,
                  const std::vector<std::pair<QString, QString>> &rows);

    /// Display a raw REL value's full details (e.g. a Dataset DataArray).
    /// Renders `name` + the same engine value fields an Equation/Expression
    /// shows (Indep / Kind / Dimension / Data Shape / Data Type / Unit).
    void ShowRelValue(const QString &name, const xequation::EquationValue &value);

  private:
    /// Append a row (field | value) to the table.  When red, the value column
    /// is shown in red.
    void AddField(const QString &field, const QString &value, bool red = false);

    /// Render the engine value info of a REL value (mirrors builtin_library's
    /// what(x) output): Indep / Kind / Dimension / Data Shape / Data Type /
    /// Unit.  Shared by ShowEquation / ShowExpression / ShowRelValue.
    void AddRelValueInfo(const rel::Value &rel_value);

    /// Render the properties of an Equation (engine value info included).
    void ShowEquation(const xequation::Equation *equation);

    /// Render the properties of a registered Expression.
    void ShowExpression(const xequation::Expression *expression);

  private:
    /// Manager used to resolve an ObjectId into an Equation / Expression.
    const xequation::EquationManager &manager_;
    QTableWidget *table_ = nullptr;                  // read-only two-column table
    xequation::ObjectId object_id_;                  // displayed object (for refresh)
};

} // namespace gui
} // namespace xresults

