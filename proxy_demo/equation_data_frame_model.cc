#include "equation_data_frame_model.h"

#include "core/equation.h"
#include "data_frame.h"

#include <algorithm>

namespace xresults
{
namespace gui
{

using namespace xequation;

EquationDataFrameModel::EquationDataFrameModel(QObject *parent) : QAbstractTableModel(parent)
{
}

EquationDataFrameModel::~EquationDataFrameModel() = default;

const xdataset::DataFrame &EquationDataFrameModel::frame() const
{
    // Only valid when there is a REL value (caller checks HasDataFrame() first).
    return equation_value_.AsRel().data_frame();
}

void EquationDataFrameModel::SetEquation(const Equation *equation)
{
    if (!equation)
    {
        Clear();
        return;
    }

    // rel::Value::data_frame() returns a stable reference owned by the
    // underlying DataArray (see REL value.h contract: caller must keep this
    // Value alive while using the frame).  So we hold a copy of the
    // EquationValue, whose rel::Value (via shared_ptr<DataArray>) becomes the
    // frame's owner, avoiding dangling references; the Equation pointer is not
    // held.
    const EquationValue value = equation->GetValue();
    if (!value.IsRelValue())
    {
        Clear();
        return;
    }

    beginResetModel();
    equation_value_ = value;                 // hold a copy so the frame stays alive
    equation_name_ = equation->name();       // record name for removal comparison
    loaded_rows_ = std::min<std::size_t>(
        static_cast<std::size_t>(kLoadBatchSize), frame().row_count()
    );
    endResetModel();
}

void EquationDataFrameModel::Clear()
{
    beginResetModel();
    equation_value_ = EquationValue();
    equation_name_.clear();
    loaded_rows_ = 0;
    endResetModel();
}

void EquationDataFrameModel::OnEquationRemoving(const Equation *removed)
{
    // kEquationRemoving is fired before erase; the Equation* is still valid here.
    if (!removed || removed->name() != equation_name_)
    {
        return;
    }
    Clear();
}

void EquationDataFrameModel::OnEquationUpdated(const Equation *equation,
                                               bitmask::bitmask<EquationUpdateFlag> /*flags*/)
{
    if (!equation || equation->name() != equation_name_)
    {
        return;
    }
    // The value may have changed (recomputed after redefinition); reload the DataFrame.
    SetEquation(equation);
}

bool EquationDataFrameModel::HasDataFrame() const
{
    return equation_value_.IsRelValue();
}

std::size_t EquationDataFrameModel::total_row_count() const
{
    return HasDataFrame() ? frame().row_count() : 0;
}

int EquationDataFrameModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    return static_cast<int>(loaded_rows_);
}

int EquationDataFrameModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return 0;
    }
    // First column "#" (multi-index); the rest are DataFrame headers.
    return static_cast<int>(frame().headers().size()) + 1;
}

QVariant EquationDataFrameModel::data(const QModelIndex &index, int role) const
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

QVariant EquationDataFrameModel::headerData(
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

bool EquationDataFrameModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !HasDataFrame())
    {
        return false;
    }
    return loaded_rows_ < frame().row_count();
}

void EquationDataFrameModel::fetchMore(const QModelIndex &parent)
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
