#pragma once

#include <QWidget>

#include <string>
#include <vector>

#include "core/equation_manager.h"

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

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
// Layout:
//   - a title QLabel on top showing the object name (the Equation name, or
//     "Expression" for a registered expression);
//   - a read-only two-column tree below (field | value).  List-valued fields
//     (Dependencies / Dependents / Indep ...) are parent rows whose children
//     enumerate the entries, so the value can be expanded / collapsed.
//
// The tree is non-editable (NoEditTriggers) and display-only.  Refresh via
// SetObject() when the selected object changes; external code connects the
// manager's kEquationRemoving / kEquationUpdated / kExpressionUpdated /
// kExpressionRemoving signals to the slots below; this widget decides whether
// it displays that object.
// =========================================================================

class ExpressionPropertyWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit ExpressionPropertyWidget(const xequation::EquationManager &manager,
                                      QWidget *parent = nullptr);
    ~ExpressionPropertyWidget() override;

    /// Set the object to display (refresh).  A default-constructed (nil) id
    /// clears the tree and shows a placeholder.
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

    /// External code connects both engines' managers' kExpressionRemoving
    /// signal here (fired before deletion).  If the removed expression is the
    /// one being displayed, clears.
    void OnExpressionRemoving(const xequation::Expression *expression);

  private:
    /// Append a top-level row (field | value) to the tree.  When red, the
    /// value column is shown in red.  Returns the row so list values can add
    /// child entries below it.
    QTreeWidgetItem *AddField(const QString &field, const QString &value,
                              bool red = false);

    /// Add a top-level row whose value is a list of names (e.g.
    /// Dependencies).  The row shows "(N)", with each name as a child row so
    /// the value is expandable.
    void AddListField(const QString &field, const std::vector<std::string> &items);

    /// Render the engine value info of a REL value (mirrors builtin_library's
    /// what(x) output): Indep / Kind / Dimension / Data Shape / Data Type /
    /// Unit.  Shared by ShowEquation / ShowExpression.
    void AddRelValueInfo(const rel::Value &rel_value);

    /// Render the properties of an Equation (engine value info included).
    void ShowEquation(const xequation::Equation *equation);

    /// Render the properties of a registered Expression.
    void ShowExpression(const xequation::Expression *expression);

  private:
    /// Manager used to resolve an ObjectId into an Equation / Expression.
    const xequation::EquationManager &manager_;
    QLabel *name_label_ = nullptr;       // title: equation name / "Expression"
    QTreeWidget *tree_ = nullptr;        // read-only two-column field tree
    xequation::ObjectId object_id_;      // displayed object (for refresh)
};

} // namespace gui
} // namespace xresults

