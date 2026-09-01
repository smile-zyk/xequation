#include "expression_data_frame_model.h"

#include "data_frame.h"

#include <algorithm>

namespace xresults
{
namespace gui
{

using namespace xequation;

ExpressionDataFrameModel::ExpressionDataFrameModel(const EquationManager &manager, QObject *parent)
    : QAbstractTableModel(parent), manager_(manager)
{
}

ExpressionDataFrameModel::~ExpressionDataFrameModel() = default;

const xdataset::DataFrame &ExpressionDataFrameModel::frame() const
{
    // Only valid when there is a REL value (caller checks HasDataFrame() first).
    return equation_value_.AsRel().data_frame();
}

void ExpressionDataFrameModel::SetObject(const ObjectId &object_id)
{
    object_id_ = object_id;

    // A registered expression wins over an equation with the same id (the
    // Equation identity is its group_id; the two namespaces are disjoint).
    const Expression *expression = manager_.GetExpression(object_id);
    if (expression)
    {
        const EquationValue value = expression->result.value;
        if (expression->result.status != ResultStatus::kSuccess || !value.IsRelValue())
        {
            Clear();
            return;
        }

        // rel::Value::data_frame() returns a stable reference owned by the
        // underlying DataArray (see REL value.h contract: caller must keep this
        // Value alive while using the frame).  So we hold a copy of the
        // EquationValue, whose rel::Value (via shared_ptr<DataArray>) becomes the
        // frame's owner, avoiding dangling references; the id is kept only for
        // event matching.
        beginResetModel();
        equation_value_ = value;   // hold a copy so the frame stays alive
        loaded_rows_ = std::min<std::size_t>(
            static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
        );
        endResetModel();
        return;
    }

    // Otherwise resolve as an equation (group id -> single-equation group).
    const EquationGroup *group = manager_.GetEquationGroup(object_id);
    if (!group)
    {
        Clear();
        return;
    }
    const Equation *equation = group->FirstEquation();
    if (!equation)
    {
        Clear();
        return;
    }

    // rel::Value::data_frame() returns a stable reference owned by the
    // underlying DataArray (see REL value.h contract: caller must keep this
    // Value alive while using the frame).  So we hold a copy of the
    // EquationValue, whose rel::Value (via shared_ptr<DataArray>) becomes the
    // frame's owner, avoiding dangling references; the id is kept only for
    // event matching.
    const EquationValue value = equation->GetValue();
    if (!value.IsRelValue())
    {
        Clear();
        return;
    }

    beginResetModel();
    equation_value_ = value;                 // hold a copy so the frame stays alive
    loaded_rows_ = std::min<std::size_t>(
        static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
    );
    endResetModel();
}

void ExpressionDataFrameModel::SetValue(const EquationValue &value)
{
    if (!value.IsRelValue())
    {
        Clear();
        return;
    }

    beginResetModel();
    equation_value_ = value;   // hold a copy so the frame stays alive
    object_id_ = ObjectId();   // a bare value is not object-bound
    loaded_rows_ = std::min<std::size_t>(
        static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
    );
    endResetModel();
}

void ExpressionDataFrameModel::Clear()
{
    beginResetModel();
    equation_value_ = EquationValue();
    object_id_ = ObjectId();
    loaded_rows_ = 0;
    endResetModel();
}

void ExpressionDataFrameModel::OnEquationRemoving(const Equation *removed)
{
    // kEquationRemoving is fired before erase; the Equation* is still valid here.
    if (!removed || object_id_.is_nil())
    {
        return;
    }
    // An Equation's identity is its group_id.
    if (removed->group_id() != object_id_)
    {
        return;
    }
    Clear();
}

void ExpressionDataFrameModel::OnEquationUpdated(const Equation *equation,
                                               bitmask::bitmask<EquationUpdateFlag> /*flags*/)
{
    if (!equation || object_id_.is_nil())
    {
        return;
    }
    // Already-removed objects are handled by OnEquationRemoving; a registered
    // expression never fires kEquationUpdated, so comparing group_id is enough.
    if (equation->group_id() != object_id_)
    {
        return;
    }
    // The value may have changed (recomputed after redefinition); reload the DataFrame.
    SetObject(object_id_);
}

void ExpressionDataFrameModel::OnExpressionUpdated(const Expression *expression,
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
    // The value may have changed; reload the DataFrame.
    SetObject(object_id_);
}

bool ExpressionDataFrameModel::HasDataFrame() const
{
    return equation_value_.IsRelValue();
}

std::size_t ExpressionDataFrameModel::total_row_count() const
{
    return HasDataFrame() ? frame().row_count() : 0;
}

int ExpressionDataFrameModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    return static_cast<int>(loaded_rows_);
}

int ExpressionDataFrameModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    // First column "#" (multi-index); the rest are DataFrame headers.
    return static_cast<int>(frame().headers().size()) + 1;
}

QVariant ExpressionDataFrameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !HasDataFrame())
    {
        return QVariant();
    }

    if (index.row() < 0 || static_cast<std::size_t>(index.row()) >= loaded_rows_)
    {
        return QVariant();
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole)
    {
        return QVariant();
    }

    const xdataset::DataFrameRow &row =
        frame().GetRow(static_cast<xdataset::Index>(index.row()));

    if (index.column() == 0)
    {
        return QString::fromStdString(row.FormatMultiIndex());
    }

    const int field_index = index.column() - 1;
    if (field_index < 0 || static_cast<std::size_t>(field_index) >= row.fields.size())
    {
        return QVariant();
    }

    return QString::fromStdString(row.fields[static_cast<std::size_t>(field_index)].to_string());
}

QVariant ExpressionDataFrameModel::headerData(
    int section, Qt::Orientation orientation, int role
) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || !HasDataFrame())
    {
        return QVariant();
    }

    if (section == 0)
    {
        return QString("#");
    }

    const std::vector<std::string> &headers = frame().headers();
    const int header_index = section - 1;
    if (header_index < 0 || static_cast<std::size_t>(header_index) >= headers.size())
    {
        return QVariant();
    }

    return QString::fromStdString(headers[static_cast<std::size_t>(header_index)]);
}

bool ExpressionDataFrameModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return false;
    }
    return loaded_rows_ < frame().row_count();
}

void ExpressionDataFrameModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || !HasDataFrame() || !canFetchMore(parent))
    {
        return;
    }

    const std::size_t begin = loaded_rows_;
    const std::size_t end = std::min(
        begin + static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
    );
    if (begin >= end)
    {
        return;
    }

    beginInsertRows(QModelIndex(), static_cast<int>(begin), static_cast<int>(end - 1));
    loaded_rows_ = end;
    endInsertRows();
}

} // namespace gui
} // namespace xresults
