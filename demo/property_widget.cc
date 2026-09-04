#include "property_widget.h"

#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "core/equation_value.h"

#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "xdataset_predefine.h"

#include "dataset.h"     // xdataset::Dataset
#include "block.h"       // xdataset::Block
#include "environment.h" // rel::Environment (dataset registry)

#include <algorithm>
#include <string>
#include <vector>

namespace xresults
{
namespace gui
{

using namespace xequation;

PropertyWidget::PropertyWidget(const EquationManager &manager,
                                                   QWidget *parent)
    : QWidget(parent), manager_(manager)
{
    setWindowTitle("Properties");

    // ---- title label: object name (Equation name / "Expression") ---------
    name_label_ = new QLabel(this);
    // Compact subtitle-like title: smaller than the default body font.
    QFont name_font = name_label_->font();
    name_font.setBold(true);
    name_font.setPointSizeF(name_font.pointSizeF() - 1.0);
    name_label_->setFont(name_font);
    name_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    name_label_->setContentsMargins(2, 2, 0, 0);

    // ---- read-only two-column field tree ---------------------------------
    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({QStringLiteral("Field"), QStringLiteral("Value")});
    tree_->setRootIsDecorated(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setFocusPolicy(Qt::ClickFocus);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(name_label_);
    layout->addWidget(tree_, 1);

    // Initially empty (no selected object); filled by SetObject.
    SetObject(ObjectId());
}

PropertyWidget::~PropertyWidget() = default;

void PropertyWidget::SetObject(const ObjectId &object_id)
{
    object_id_ = object_id;

    tree_->clear();

    // Registered expression wins over an equation with the same id (the two
    // namespaces are disjoint).
    const Expression *expression = manager_.GetExpression(object_id);
    if (expression)
    {
        // Title = the expression text itself.
        name_label_->setText(QString::fromStdString(expression->content));
        name_label_->show();
        tree_->show();
        ShowExpression(expression);
        return;
    }

    const Equation *equation = manager_.GetEquationById(object_id);
    if (!equation)
    {
        // Nothing selected (or the object is gone): the whole panel is
        // hidden -- no title, no tree frame.
        name_label_->clear();
        name_label_->hide();
        tree_->hide();
        return;
    }
    name_label_->setText(QString::fromStdString(equation->name));
    name_label_->show();
    tree_->show();
    ShowEquation(equation);
}

void PropertyWidget::ShowDatasetNode(const QString &dataset_name)
{
    object_id_ = ObjectId();  // no ObjectId behind a Dataset node

    tree_->clear();

    const xdataset::Dataset *dataset =
        rel::Environment::FindDataset(dataset_name.toStdString());
    if (!dataset)
    {
        name_label_->clear();
        name_label_->hide();
        tree_->hide();
        return;
    }

    const bool is_default =
        rel::Environment::DefaultDataset() &&
        rel::Environment::DefaultDataset()->name() == dataset->name();

    name_label_->setText(QString::fromStdString(dataset->name()));
    name_label_->show();
    tree_->show();
    ShowDataset(dataset, is_default);
}

void PropertyWidget::ShowBlockNode(const QString &dataset_name,
                                             const QString &block_path)
{
    object_id_ = ObjectId();  // no ObjectId behind a Block node

    tree_->clear();

    const xdataset::Dataset *dataset =
        rel::Environment::FindDataset(dataset_name.toStdString());
    if (!dataset)
    {
        name_label_->clear();
        name_label_->hide();
        tree_->hide();
        return;
    }

    const xdataset::Block *block = nullptr;
    try
    {
        block = &dataset->GetBlock(block_path.toStdString());
    }
    catch (const std::exception &)
    {
        block = nullptr;
    }
    if (!block)
    {
        name_label_->clear();
        name_label_->hide();
        tree_->hide();
        return;
    }

    name_label_->setText(QString::fromStdString(block->name()));
    name_label_->show();
    tree_->show();
    ShowBlock(block);
}

void PropertyWidget::AddRelValueInfo(const rel::Value &rel_value)
{
    // ---- REL value: mirrors builtin_library's what(x) output ----
    AddListField("Indep", rel_value.indep_names());
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

void PropertyWidget::ShowEquation(const Equation *equation)
{
    if (!equation)
    {
        AddField("Status", "No equation selected.");
        return;
    }

    // =====================================================================
    // 1. Equation meta info
    // =====================================================================
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

    // Dependencies / dependents (active dependency-graph edges).
    AddListField("Dependencies", manager_.GetDependencies(equation->id));
    AddListField("Dependents", manager_.GetDependents(equation->id));

    // =====================================================================
    // 2. Engine value info (not shown on compute failure, to avoid misleading)
    // =====================================================================
    const EquationValue value = manager_.GetEquationValue(equation->name);

    if (equation->status == ResultStatus::kSuccess && value.HasValue())
    {
        AddRelValueInfo(value.Value());
        // Value body is not shown here: the DataFrame tree handles it, avoiding
        // duplicating lengthy content.
    }
    // Compute failure: value unavailable (already shown as red Message above); nothing here.
}

void PropertyWidget::ShowExpression(const Expression *expression)
{
    if (!expression)
    {
        AddField("Status", "No expression selected.");
        return;
    }

    // =====================================================================
    // Registered-expression meta info
    // =====================================================================
    // No "Expression" row: the title label already says "Expression" and the
    // content text is the tab label / tree node text.
    AddField("Status", QString::fromStdString(ResultStatusConverter::ToString(expression->result.status)));

    // Message row: only shown when non-empty; highlighted red on compute failure.
    const std::string &message = expression->result.message;
    if (!message.empty())
    {
        const bool has_error = expression->result.status != ResultStatus::kSuccess;
        AddField("Message", QString::fromStdString(message), has_error);
    }

    // Dependencies: a registered expression reads equations / data arrays
    // like an equation does, so show the same active graph edges.  It has no
    // dependents of its own (nothing can reference an expression by name), so
    // no Dependents row is shown.
    AddListField("Dependencies", manager_.GetDependencies(expression->id));

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

void PropertyWidget::ShowDataset(const xdataset::Dataset *dataset, bool is_default)
{
    if (!dataset)
    {
        AddField("Status", "No dataset selected.");
        return;
    }

    AddField("Default Dataset", is_default ? "Yes" : "No");

    const std::vector<std::string> block_paths = dataset->GetAllBlockPaths();
    AddListField("Variable Blocks", block_paths);

    const std::string &source_path = dataset->source_path();
    if (!source_path.empty())
    {
        AddField("Dataset Path", QString::fromStdString(source_path));
    }
}

void PropertyWidget::ShowBlock(const xdataset::Block *block)
{
    if (!block)
    {
        AddField("Status", "No block selected.");
        return;
    }
    // ---- 2. Independents / dependents ---------------------------------------
    AddListField("Independents", block->independents());
    AddListField("Dependents", block->dependents());
    AddField("In Dataset", QString::fromStdString(block->dataset_name()));
}

void PropertyWidget::OnEquationRemoving(const Equation *equation)
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

void PropertyWidget::OnEquationUpdated(const Equation *equation,
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

void PropertyWidget::OnExpressionUpdated(const Expression *expression,
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

void PropertyWidget::OnExpressionRemoving(const Expression *expression)
{
    // kExpressionRemoving is fired before erase; the Expression* is valid here.
    if (!expression || object_id_.is_nil())
    {
        return;
    }
    if (expression->id != object_id_)
    {
        return;
    }
    SetObject(ObjectId());
}

QTreeWidgetItem *PropertyWidget::AddField(const QString &field,
                                                    const QString &value, bool red)
{
    auto *item = new QTreeWidgetItem(tree_);
    item->setText(0, field);
    item->setText(1, value);

    QFont field_font = item->font(0);
    field_font.setBold(true);
    item->setFont(0, field_font);

    if (red)
    {
        item->setForeground(1, QColor(0xd3, 0x2f, 0x2f));   // #d32f2f
        QFont value_font = item->font(1);
        value_font.setBold(true);
        item->setFont(1, value_font);
    }
    return item;
}

void PropertyWidget::AddListField(const QString &field,
                                            const std::vector<std::string> &items)
{
    // A parent row shows the count; each entry is an expandable child row.
    QTreeWidgetItem *parent =
        AddField(field, QStringLiteral("(%1)").arg(static_cast<int>(items.size())));
    for (const std::string &name : items)
    {
        auto *child = new QTreeWidgetItem(parent);
        child->setText(1, QString::fromStdString(name));
    }
}

} // namespace gui
} // namespace xresults
