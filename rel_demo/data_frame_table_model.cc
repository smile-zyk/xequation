#include "data_frame_table_model.h"

#include "core/equation.h"
#include "core/equation_manager.h"
#include "data_frame.h"

#include <algorithm>

namespace xequation
{
namespace rel_demo
{

DataFrameTableModel::DataFrameTableModel(QObject *parent) : QAbstractTableModel(parent)
{
}

DataFrameTableModel::~DataFrameTableModel() = default;

void DataFrameTableModel::SetEquation(const Equation *equation)
{
    if (!equation)
    {
        Clear();
        return;
    }

    // 上游 rel::Value::data_frame() 返回的是由底层 DataArray 拥有的
    // stable reference（见 REL value.h 契约：caller must keep this Value
    // alive while using the frame）。因此这里【持有 EquationValue 副本】，
    // 使其内部的 rel::Value (经 shared_ptr<DataArray>) 成为 frame 的 owner，
    // 避免引用悬垂；不持有 Equation 指针。
    const EquationValue value = equation->GetValue();
    if (!value.IsRelValue())
    {
        Clear();
        return;
    }

    BindToManager(equation);                 // 绑定当前方程所属 manager，处理删除

    beginResetModel();
    equation_value_ = value;                 // 持有副本，保证 frame 存活
    const xdataset::DataFrame &frame = equation_value_.AsRel().data_frame();
    data_frame_ = &frame;                    // stable reference 缓存
    loaded_rows_ = std::min<std::size_t>(
        static_cast<std::size_t>(kLoadBatchSize), frame.row_count()
    );
    endResetModel();
}

void DataFrameTableModel::Clear()
{
    // 断开删除信号：清空后不再需要感知任何方程的移除。
    removing_connection_.disconnect();
    equation_name_.clear();

    beginResetModel();
    equation_value_ = EquationValue();
    data_frame_ = nullptr;
    loaded_rows_ = 0;
    endResetModel();
}

void DataFrameTableModel::BindToManager(const Equation *equation)
{
    const EquationManager *manager = equation->manager();
    if (!manager)
    {
        removing_connection_.disconnect();
        equation_name_.clear();
        return;
    }

    const std::string name = equation->name();
    // 已绑定到同一个名字（通常是同一 manager 的同一方程），无需重建连接。
    if (removing_connection_.connected() && equation_name_ == name)
    {
        return;
    }

    // Schema: kEquationRemoving = signal<void(const Equation *)>，删除前触发。
    removing_connection_ = manager->signals_manager().ConnectScoped<EquationEvent::kEquationRemoving>(
        [this](const Equation *removed)
        {
            OnEquationRemoving(removed);
        }
    );
    equation_name_ = name;
}

void DataFrameTableModel::OnEquationRemoving(const Equation *removed)
{
    // kEquationRemoving 在 erase 之前发出，此处 Equation* 仍有效（可安全读 name）。
    if (!removed || removed->name() != equation_name_)
    {
        return;
    }
    Clear();
}

bool DataFrameTableModel::HasDataFrame() const
{
    return data_frame_ != nullptr;
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
