#include "data_frame_model.h"

#include "block.h"        // xdataset::Block::GetOrCreateDataFrame
#include "data_frame.h"
#include "measurement.h"

#include <QVariant>

#include <algorithm>
#include <cmath>

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

// ---- display formatting -------------------------------------------------

void DataFrameModel::SetFormatOptions(const xdataset::FormatOptions &options)
{
    format_options_ = options;
    // The rendered strings change, so drop the cache.  A full reset is not
    // needed (the shape is unchanged) but Qt must repaint, so invalidate the
    // whole viewport.
    InvalidateCellCache();
    if (HasDataFrame())
    {
        emit dataChanged(index(0, 0),
                         index(static_cast<int>(loaded_rows_) - 1,
                               columnCount() - 1));
    }
}

void DataFrameModel::InvalidateCellCache()
{
    cell_cache_.clear();
    cell_cached_.clear();
    cache_columns_ = 0;
}

void DataFrameModel::ReserveCellCache(std::size_t rows)
{
    const int columns = columnCount();
    if (columns <= 0 || rows == 0)
    {
        InvalidateCellCache();
        return;
    }
    const int total = static_cast<int>(rows) * columns;
    if (cache_columns_ == columns && cell_cache_.size() == total)
    {
        return;   // already laid out for this shape
    }

    if (cache_columns_ != columns)
    {
        // Column count changed: the row-major layout no longer lines up, so
        // every cached string is meaningless.
        cell_cache_.resize(total);
        cell_cached_.fill(false, total);
    }
    else
    {
        // Same columns, more rows (fetchMore): keep the already-rendered
        // cells and only mark the newly appended region as uncached.
        const int old_total = cell_cache_.size();
        cell_cache_.resize(total);
        cell_cached_.resize(total);
        for (int i = old_total; i < total; ++i)
        {
            cell_cached_[i] = false;
        }
    }
    cache_columns_ = columns;
}

QString DataFrameModel::FormatMultiIndex(const xdataset::DataFrameRow &row)
{
    // Same output as xdataset::DataFrameRow::FormatMultiIndex(), but built
    // with QString::number instead of std::ostringstream (which was one
    // locale-imbued stream construction per cell in column 0).
    QString out;
    out.reserve(static_cast<int>(row.multi_index.size()) * 4);
    for (std::size_t i = 0; i < row.multi_index.size(); ++i)
    {
        if (i > 0)
        {
            out += QLatin1Char(',');
        }
        out += QString::number(row.multi_index[i]);
    }
    return out;
}

QString DataFrameModel::FormatCell(int row, int column) const
{
    const xdataset::DataFrameRow &r =
        frame().GetRow(static_cast<xdataset::Index>(row));

    if (column == 0)
    {
        return FormatMultiIndex(r);
    }

    const int field_index = column - 1;
    if (field_index < 0 ||
        static_cast<std::size_t>(field_index) >= r.fields.size())
    {
        return QString();
    }

    const xdataset::Measurement &field =
        r.fields[static_cast<std::size_t>(field_index)];

    // Each cell is formatted on its own, with THIS model's options: the unit
    // is resolved per value (Unit::best_display), so a wide-dynamic-range
    // column shows "1.5 KV" next to "2 mV" instead of forcing one unit on the
    // whole column.  The options are passed per call -- rendering is a pure
    // function, so no global state is touched.
    return QString::fromStdString(field.to_string(format_options_));
}

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
            static_cast<std::size_t>(kInitialLoadRows), frame().row_count()
        );
        InvalidateCellCache();
        ReserveCellCache(loaded_rows_);
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
        static_cast<std::size_t>(kInitialLoadRows), frame().row_count()
    );
    InvalidateCellCache();
    ReserveCellCache(loaded_rows_);
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
        static_cast<std::size_t>(kInitialLoadRows), frame().row_count()
    );
    InvalidateCellCache();
    ReserveCellCache(loaded_rows_);
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
              static_cast<std::size_t>(kInitialLoadRows), block_frame_->row_count())
        : 0;
    InvalidateCellCache();
    ReserveCellCache(loaded_rows_);
    endResetModel();
}

void DataFrameModel::Clear()
{
    beginResetModel();
    equation_value_ = EquationValue();
    block_frame_ = nullptr;
    loaded_rows_ = 0;
    InvalidateCellCache();
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

    // Memoised rendering: Qt asks for the same cell repeatedly (size hint,
    // paint, delegate, selection) and each render costs a unit lookup plus a
    // number-to-string conversion.
    const int columns = columnCount();
    if (columns > 0 && cache_columns_ == columns)
    {
        const int slot = index.row() * columns + index.column();
        if (slot >= 0 && slot < cell_cached_.size())
        {
            if (!cell_cached_[slot])
            {
                cell_cache_[slot] = FormatCell(index.row(), index.column());
                cell_cached_[slot] = true;
            }
            return cell_cache_[slot];
        }
    }

    return FormatCell(index.row(), index.column());
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

void DataFrameModel::EnsureLoadedRows(std::size_t rows)
{
    if (!HasDataFrame())
    {
        return;
    }

    const std::size_t end = std::min(rows, frame().row_count());
    if (end <= loaded_rows_)
    {
        return;   // nothing new to load
    }

    const std::size_t begin = loaded_rows_;

    // Grow the cell cache for the new rows.  Existing entries keep their
    // row-major slot because the column count is unchanged.
    ReserveCellCache(end);

    beginInsertRows(QModelIndex(), static_cast<int>(begin), static_cast<int>(end - 1));
    loaded_rows_ = end;
    endInsertRows();
}

void DataFrameModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || !HasDataFrame() || !canFetchMore(parent))
    {
        return;
    }

    // A scroll batch on top of whatever is already loaded.  Computed in
    // std::size_t so a very large loaded_rows_ cannot overflow int.
    EnsureLoadedRows(loaded_rows_ + static_cast<std::size_t>(kLoadBatchSize));
}

} // namespace gui
} // namespace xresults
