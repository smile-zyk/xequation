#include "expression_property_widget.h"

#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "core/equation_value.h"

#include <boost/uuid/uuid_io.hpp>

#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "xdataset_predefine.h"

#include <sstream>
#include <string>

#include <tsl/ordered_set.h>

namespace xresults
{
namespace gui
{

using namespace xequation;

namespace
{
/// Render a tsl::ordered_set<std::string> as "[a, b, c]" form.
template <typename Set>
QString FormatSet(const Set &items)
{
    std::ostringstream oss;
    oss << '[';
    bool first = true;
    for (const auto &item : items)
    {
        if (!first)
        {
            oss << ", ";
        }
        oss << item;
        first = false;
    }
    oss << ']';
    return QString::fromStdString(oss.str());
}
} // namespace

ExpressionPropertyWidget::ExpressionPropertyWidget(const EquationManager &manager, QWidget *parent)
    : QWidget(parent), manager_(manager)
{
    setWindowTitle("Equation Property");

    // Read-only two-column table: field | value.
    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({"Field", "Value"});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);        // no editing
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);        // select whole row
    table_->setSelectionMode(QAbstractItemView::SingleSelection);      // single selection
    table_->setFocusPolicy(Qt::ClickFocus);
    table_->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(table_);

    // Initially empty (no selected object); filled by SetObject.
    SetObject(ObjectId());
}

ExpressionPropertyWidget::~ExpressionPropertyWidget() = default;

void ExpressionPropertyWidget::ShowInfo(const QString &heading,
                                        const std::vector<std::pair<QString, QString>> &rows)
{
    // Break the ObjectId binding: manager refresh signals (kEquation* /
    // kExpression*) compare against object_id_ and ignore nil ids.
    object_id_ = ObjectId();
    table_->setRowCount(0);

    if (!heading.isEmpty())
    {
        AddField(heading, QString());
    }
    for (const auto &field : rows)
    {
        AddField(field.first, field.second);
    }
}

void ExpressionPropertyWidget::SetObject(const ObjectId &object_id)
{
    object_id_ = object_id;

    // Clear the table.
    table_->setRowCount(0);

    // No selected object: show a placeholder row.
    if (object_id.is_nil())
    {
        AddField("Status", "No object selected.");
        return;
    }

    // Registered expression wins over an equation with the same id (the two
    // namespaces are disjoint).
    const Expression *expression = manager_.GetExpression(object_id);
    if (expression)
    {
        ShowExpression(expression);
        return;
    }

    // Otherwise resolve as an equation (by id).
    const Equation *equation = manager_.GetEquationById(object_id);
    if (!equation)
    {
        AddField("Status", "Object no longer exists.");
        return;
    }
    ShowEquation(equation);
}

void ExpressionPropertyWidget::AddRelValueInfo(const rel::Value &rel_value)
{
    // ---- REL value: mirrors builtin_library's what(x) output ----
    AddField("Indep", FormatSet(rel_value.indep_names()));
    AddField(
        "Kind",
        QString::fromStdString(rel_value.is_dependent() ? "Dependent" : "Independent")
    );
    AddField("Dimension", QString::fromStdString(rel_value.dimension_spec().to_string()));
    AddField("Data Shape", QString::fromStdString(rel_value.data_shape().to_string()));
    AddField(
        "Data Type",
        QString::fromStdString(xdataset::DataTypeToString(rel_value.data_type()))
    );
    if (rel_value.unit().has_dimension())
    {
        AddField("Unit", QString::fromStdString(rel_value.unit().to_string()));
    }
}

void ExpressionPropertyWidget::ShowRelValue(const QString &name,
                                            const EquationValue &value)
{
    object_id_ = ObjectId();   // not manager-bound; refresh signals ignore nil
    table_->setRowCount(0);

    AddField("Name", name);
    if (!value.HasValue())
    {
        AddField("Status", "No value.");
        return;
    }

    // Same engine value details an Equation / Expression shows.
    AddRelValueInfo(value.Value());
}

void ExpressionPropertyWidget::ShowEquation(const Equation *equation)
{
    if (!equation)
    {
        AddField("Status", "No equation selected.");
        return;
    }

    // =====================================================================
    // 1. Equation meta info
    // =====================================================================
    AddField("Name", QString::fromStdString(equation->name));
    AddField("Tag", QString::fromStdString(equation->tag));
    AddField("Expression", QString::fromStdString(equation->content));
    AddField("Status", QString::fromStdString(ResultStatusConverter::ToString(equation->status)));

    // Message row: only shown when non-empty; highlighted red on compute failure
    // (no duplicate Error row).
    const std::string &message = equation->message;
    if (!message.empty())
    {
        const bool has_error = equation->status != ResultStatus::kSuccess;
        AddField("Message", QString::fromStdString(message), has_error);
    }

    // Dependencies / dependents (expression dependency graph).
    AddField("Dependencies", FormatSet(manager_.GetEquationDependencies(equation->name)));
    AddField("Dependents", FormatSet(manager_.GetEquationDependents(equation->name)));

    // =====================================================================
    // 2. Engine value info (not shown on compute failure, to avoid misleading)
    // =====================================================================
    const EquationValue value = manager_.GetEquationValue(equation->name);

    if (equation->status == ResultStatus::kSuccess && value.HasValue())
    {
        AddRelValueInfo(value.Value());
        // Value body is not shown here: the DataFrame table handles it, avoiding
        // duplicating lengthy content.
    }
    // Compute failure: value unavailable (already shown as red Message above); nothing here.
}

void ExpressionPropertyWidget::ShowExpression(const Expression *expression)
{
    if (!expression)
    {
        AddField("Status", "No expression selected.");
        return;
    }

    // =====================================================================
    // Registered-expression meta info
    // =====================================================================
    AddField("ID", QString::fromStdString(boost::uuids::to_string(expression->id)));
    AddField("Tag", QString::fromStdString(expression->tag));
    AddField("Expression", QString::fromStdString(expression->content));
    AddField("Status", QString::fromStdString(ResultStatusConverter::ToString(expression->result.status)));

    // Message row: only shown when non-empty; highlighted red on compute failure.
    const std::string &message = expression->result.message;
    if (!message.empty())
    {
        const bool has_error = expression->result.status != ResultStatus::kSuccess;
        AddField("Message", QString::fromStdString(message), has_error);
    }

    // Dependencies (a registered expression has no dependents of its own).
    AddField("Dependencies", FormatSet(expression->dependencies));

    // =====================================================================
    // 2. Engine value info (not shown on compute failure)
    // =====================================================================
    const EquationValue &value = expression->result.value;
    if (expression->result.status == ResultStatus::kSuccess && value.HasValue())
    {
        AddRelValueInfo(value.Value());
    }
    // Compute failure: value unavailable (already shown as red Message above); nothing here.
}

void ExpressionPropertyWidget::OnEquationRemoving(const Equation *equation)
{
    // kEquationRemoving is fired before erase; the Equation* is still valid here.
    if (!equation || object_id_.is_nil())
    {
        return;
    }
    if (equation->id != object_id_)
    {
        return;
    }
    SetObject(ObjectId());
}

void ExpressionPropertyWidget::OnEquationUpdated(const Equation *equation,
                                               bitmask::bitmask<EquationUpdateFlag> /*flags*/)
{
    if (!equation || object_id_.is_nil())
    {
        return;
    }
    // Already-removed objects are handled by OnEquationRemoving; a registered
    // expression never fires kEquationUpdated, so comparing id is enough.
    if (equation->id != object_id_)
    {
        return;
    }
    // Value/properties may have changed; reload.
    SetObject(object_id_);
}

void ExpressionPropertyWidget::OnExpressionUpdated(const Expression *expression,
                                                 bitmask::bitmask<ExpressionUpdateFlag> /*flags*/)
{
    if (!expression || object_id_.is_nil())
    {
        return;
    }
    if (expression->id != object_id_)
    {
        return;
    }
    // Value/properties may have changed; reload.
    SetObject(object_id_);
}

void ExpressionPropertyWidget::AddField(const QString &field, const QString &value, bool red)
{
    const int row = table_->rowCount();
    table_->insertRow(row);

    QTableWidgetItem *field_item = new QTableWidgetItem(field);
    QTableWidgetItem *value_item = new QTableWidgetItem(value);

    QFont field_font = field_item->font();
    field_font.setBold(true);
    field_item->setFont(field_font);

    if (red)
    {
        value_item->setForeground(QColor(0xd3, 0x2f, 0x2f));   // #d32f2f
        value_item->setFont(field_font);
    }

    table_->setItem(row, 0, field_item);
    table_->setItem(row, 1, value_item);
}

} // namespace gui
} // namespace xresults
