#pragma once

#include <QWidget>

#include "core/equation.h"
#include "core/equation_signals_manager.h"

class QTableWidget;
class QTableWidgetItem;

namespace xresults
{
namespace gui
{

// =========================================================================
// ExpressionPropertyWidget -- property display widget for a single Equation
// (resident; refreshable).
//
// Shows the properties in a read-only two-column table (field | value):
//   1. Equation meta info (mirrors EquationBrowser): Name / Expression / Type /
//      Status / Message / Dependencies / Dependents.
//   2. Engine value info:
//      - REL engine: mirrors builtin_library's what(x) output -- Indep / Kind /
//        Dimension / Data Shape / Data Type / Unit, then the Value on demand.
//      - Python engine: besides Equation meta info, shows the Python object
//        type name (class name).
//
// The table is non-editable (NoEditTriggers) and display-only.  Refresh via
// SetEquation() when the selected equation changes; external code connects
// both engines' managers' kEquationRemoving / kEquationUpdated signals to the
// two slots below; this widget decides whether it displays that equation.
// =========================================================================

class ExpressionPropertyWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit ExpressionPropertyWidget(QWidget *parent = nullptr);
    ~ExpressionPropertyWidget() override;

    /// Set the equation to display (refresh).  Passing null clears the table and
    /// shows a placeholder.
    void SetEquation(const xequation::Equation *equation);

    /// External code connects both engines' managers' kEquationRemoving signal
    /// here (fired before deletion).  If the removed equation is the one being
    /// displayed, clears.
    void OnEquationRemoving(const xequation::Equation *equation);

    /// External code connects both engines' managers' kEquationUpdated signal
    /// here (fired on equation update).  If the updated equation is the one being
    /// displayed, reloads.
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);

  private:
    /// Append a row (field | value) to the table.  When red, the value column
    /// is shown in red.
    void AddField(const QString &field, const QString &value, bool red = false);

  private:
    QTableWidget *table_ = nullptr;             // read-only two-column table
    const xequation::Equation *equation_ = nullptr;  // displayed equation (for refresh)
};

} // namespace gui
} // namespace xresults

