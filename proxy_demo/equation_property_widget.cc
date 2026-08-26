#include "equation_property_widget.h"

#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_value.h"

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

EquationPropertyWidget::EquationPropertyWidget(QWidget *parent)
    : QWidget(parent)
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

    // Initially empty (no selected equation); filled by SetEquation.
    SetEquation(nullptr);
}

EquationPropertyWidget::~EquationPropertyWidget() = default;

void EquationPropertyWidget::SetEquation(const Equation *equation)
{
    equation_ = equation;

    // Clear the table.
    table_->setRowCount(0);

    // No selected equation: show a placeholder row.
    if (!equation)
    {
        AddField("Status", "No equation selected.");
        return;
    }

    // =====================================================================
    // 1. Equation meta info (mirrors EquationBrowser)
    // =====================================================================
    AddField("Name", QString::fromStdString(equation_->name()));
    AddField("Expression", QString::fromStdString(equation_->content()));
    AddField("Type", QString::fromStdString(ItemTypeConverter::ToString(equation_->type())));
    AddField("Status", QString::fromStdString(ResultStatusConverter::ToString(equation_->status())));

    // Message row: only shown when non-empty; highlighted red on compute failure
    // (no duplicate Error row).
    const std::string &message = equation_->message();
    if (!message.empty())
    {
        const bool has_error = equation_->status() != ResultStatus::kSuccess;
        AddField("Message", QString::fromStdString(message), has_error);
    }

    // Dependencies / dependents (expression dependency graph).
    AddField("Dependencies", FormatSet(equation_->GetDependencies()));
    AddField("Dependents", FormatSet(equation_->GetDependents()));

    // =====================================================================
    // 2. Engine value info (not shown on compute failure, to avoid misleading)
    // =====================================================================
    const EquationValue &value = equation_->GetValue();

    if (equation_->status() == ResultStatus::kSuccess && value.IsRelValue())
    {
        // ---- REL engine: mirrors builtin_library's what(x) output ----
        const rel::Value &rel_value = value.AsRel();

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
        // Value body is not shown here: the DataFrame table handles it, avoiding
        // duplicating lengthy content.
    }
    else if (equation_->status() == ResultStatus::kSuccess && value.IsPyObject())
    {
        // ---- Python engine: show the object type name (class name) ----
        AddField("Python Type", QString::fromStdString(value.AsPyObject().TypeName()));
        // Python value body is likewise not shown here, avoiding a long repr.
    }
    // Compute failure: value unavailable (already shown as red Message above); nothing here.
}

void EquationPropertyWidget::OnEquationRemoving(const Equation *equation)
{
    // kEquationRemoving is fired before erase; the Equation* is still valid here.
    if (!equation || !equation_ || equation->name() != equation_->name())
    {
        return;
    }
    SetEquation(nullptr);
}

void EquationPropertyWidget::OnEquationUpdated(const Equation *equation,
                                               bitmask::bitmask<EquationUpdateFlag> /*flags*/)
{
    if (!equation || !equation_ || equation->name() != equation_->name())
    {
        return;
    }
    // Value/properties may have changed; reload.
    SetEquation(equation);
}

void EquationPropertyWidget::AddField(const QString &field, const QString &value, bool red)
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
