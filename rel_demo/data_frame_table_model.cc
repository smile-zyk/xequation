#include "data_frame_table_model.h"

#include "core/equation_value.h"
#include "data_frame.h"

#include <algorithm>
#include <stdexcept>

namespace xequation
{
namespace rel_demo
{

DataFrameTableModel::DataFrameTableModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DataFrameTableModel::~DataFrameTableModel() = default;

void DataFrameTableModel::SetEquationValue(const EquationValue &value)
{
    if (!value.IsRelValue())
    {
        Clear();
        return;
    }

    const rel::Value &rel_value = value.AsRel();

    // 目前仅支持 Measurement / DataArray 两类 REL 值；
    // rel::Value::data_frame() 对这两种类型均可构建 DataFrame。
    try
    {
        // rel::Value::data_frame(name) -- owned DataFrame (move-only)
        SetDataFrame(rel_value.data_frame());
    }
    catch (const std::exception &)
    {
        Clear();
    }
}

void DataFrameTableModel::SetDataFrame(std::unique_ptr<xdataset::DataFrame> frame)
{
    beginResetModel();
    data_frame_ = std::move(frame);
    loaded_rows_ = data_frame_ ? std::min<std::size_t>(
                                     static_cast<std::size_t>(kLoadBatchSize),
                                     data_frame_->row_count()
                                 )
                               : 0;
    endResetModel();
}

void DataFrameTableModel::Clear()
{
    SetDataFrame(nullptr);
}

std::size_t DataFrameTableModel::total_row_count() const
{
    return data_frame_ ? data_frame_->row_count() : 0;
}

int DataFrameTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !data_frame_)
    {
        return 0;
    }
    return static_cast<int>(loaded_rows_);
}

int DataFrameTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !data_frame_)
    {
        return 0;
    }
    // 第一列 "#"（multi-index），其余为 DataFrame headers。
    return static_cast<int>(data_frame_->headers().size()) + 1;
}

QVariant DataFrameTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !data_frame_)
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
        data_frame_->GetRow(static_cast<xdataset::Index>(index.row()));

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

QVariant DataFrameTableModel::headerData(
    int section, Qt::Orientation orientation, int role
) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || !data_frame_)
    {
        return QVariant();
    }

    if (section == 0)
    {
        return QString("#");
    }

    const std::vector<std::string> &headers = data_frame_->headers();
    const int header_index = section - 1;
    if (header_index < 0 || static_cast<std::size_t>(header_index) >= headers.size())
    {
        return QVariant();
    }

    return QString::fromStdString(headers[static_cast<std::size_t>(header_index)]);
}

bool DataFrameTableModel::canFetchMore(const QModelIndex &parent) const
{
    if (parent.isValid() || !data_frame_)
    {
        return false;
    }
    return loaded_rows_ < data_frame_->row_count();
}

void DataFrameTableModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || !data_frame_ || !canFetchMore(parent))
    {
        return;
    }

    const std::size_t begin = loaded_rows_;
    const std::size_t end = std::min(
        begin + static_cast<std::size_t>(kLoadBatchSize), data_frame_->row_count()
    );
    if (begin >= end)
    {
        return;
    }

    beginInsertRows(QModelIndex(), static_cast<int>(begin), static_cast<int>(end - 1));
    loaded_rows_ = end;
    endInsertRows();
}

} // namespace rel_demo
} // namespace xequation
