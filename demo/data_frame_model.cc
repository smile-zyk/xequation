#include "data_frame_model.h"

#include "block.h"        // xdataset::Block::GetOrCreateDataFrame
#include "data_frame.h"

#include <algorithm>

namespace xresults
{
namespace gui
{

using namespace xequation;

DataFrameModel::DataFrameModel(const EquationManager &manager, QObject *parent)
    : QAbstractTableModel(parent), manager_(manager)
{
}

DataFrameModel::~DataFrameModel() = default;

const xdataset::DataFrame &DataFrameModel::frame() const
{
    // Only valid when there is a value (caller checks HasDataFrame() first).
    if (block_frame_)
    {
        return *block_frame_;
    }
    return equation_value_.Value().data_frame();
}

void DataFrameModel::SetObject(const ObjectId &object_id)
{
    block_frame_ = nullptr;  // a Block view is replaced by an ObjectId view
    // A registered expression wins over an equation with the same id (the two
    // namespaces are disjoint).
    const Expression *expression = manager_.GetExpression(object_id);
    if (expression)
    {
        const EquationValue value = expression->result.value;
        if (expression->result.status != ResultStatus::kSuccess || !value.HasValue())
        {
            Clear();
            return;
        }

        // rel::Value::data_frame() returns a stable reference owned by the
        // underlying DataArray (see REL value.h contract: caller must keep this
        // Value alive while using the frame).  So we hold a copy of the
        // EquationValue, whose rel::Value (via shared_ptr<DataArray>) becomes the
        // frame's owner, avoiding dangling references.
        beginResetModel();
        equation_value_ = value;   // hold a copy so the frame stays alive
        loaded_rows_ = std::min<std::size_t>(
            static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
        );
        endResetModel();
        return;
    }

    // Otherwise resolve as an equation (by id).
    const Equation *equation = manager_.GetEquationById(object_id);
    if (!equation)
    {
        Clear();
        return;
    }

    // rel::Value::data_frame() returns a stable reference owned by the
    // underlying DataArray (see REL value.h contract: caller must keep this
    // Value alive while using the frame).  So we hold a copy of the
    // EquationValue, whose rel::Value (via shared_ptr<DataArray>) becomes the
    // frame's owner, avoiding dangling references.
    const EquationValue value = manager_.GetEquationValue(equation->name);
    if (!value.HasValue())
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

void DataFrameModel::SetValue(const EquationValue &value)
{
    block_frame_ = nullptr;  // a Block view is replaced by a bare value
    if (!value.HasValue())
    {
        Clear();
        return;
    }

    beginResetModel();
    equation_value_ = value;   // hold a copy so the frame stays alive
    loaded_rows_ = std::min<std::size_t>(
        static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
    );
    endResetModel();
}

void DataFrameModel::SetBlock(const xdataset::Block *block)
{
    beginResetModel();
    equation_value_ = EquationValue();   // release any value-side owner
    // Block::GetOrCreateDataFrame() returns a reference to a frame the Block
    // owns and caches; taking its address is stable for the Block's lifetime.
    block_frame_ = block ? &block->GetOrCreateDataFrame() : nullptr;
    loaded_rows_ = block_frame_
        ? std::min<std::size_t>(
              static_cast<std::size_t>(kLoadBatchSize), block_frame_->row_count())
        : 0;
    endResetModel();
}

void DataFrameModel::Clear()
{
    beginResetModel();
    equation_value_ = EquationValue();
    block_frame_ = nullptr;
    loaded_rows_ = 0;
    endResetModel();
}

bool DataFrameModel::HasDataFrame() const
{
    return equation_value_.HasValue() || block_frame_ != nullptr;
}

std::size_t DataFrameModel::total_row_count() const
{
    return HasDataFrame() ? frame().row_count() : 0;
}

int DataFrameModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    return static_cast<int>(loaded_rows_);
}

int DataFrameModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    // First column "#" (multi-index); the rest are DataFrame headers.
    return static_cast<int>(frame().headers().size()) + 1;
}

QVariant DataFrameModel::data(const QModelIndex &index, int role) const
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

QVariant DataFrameModel::headerData(
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

bool DataFrameModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return false;
    }
    return loaded_rows_ < frame().row_count();
}

void DataFrameModel::fetchMore(const QModelIndex &parent)
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
