#pragma once

#include <cstddef>

#include <QAbstractTableModel>

#include "core/equation_manager.h"
#include "core/equation_value.h"

namespace xdataset
{
class DataFrame;
class Block;
}

namespace xresults
{
namespace gui
{

// =========================================================================
// ExpressionDataFrameModel -- presents the DataFrame view of an
// EquationValue(REL) as a two-dimensional table model with Qt's fetchMore
// lazy-loading.
//
// The displayed value is provided directly by the caller:
//   SetObject(ObjectId) resolves an Equation / registered Expression through
//   the injected EquationManager& and copies the resulting EquationValue as
//   the DataFrame's stable owner -- because rel::Value::data_frame() returns a
//   stable reference owned by the underlying DataArray (see REL value.h
//   contract); the caller must keep that Value alive while using the frame.
//   SetValue(EquationValue) shows a bare value (no manager binding).
//   Other kinds (or a nil id / empty value) clear the model.
//
// Lazy loading:
//   rowCount() returns the number of *loaded* rows (not the DataFrame total);
//   canFetchMore() is true while loaded_rows < DataFrame total rows;
//   fetchMore() appends kLoadBatchSize rows at a time, triggered when the
//   QTableView scrolls to the bottom (Qt built-in + view-layer complement).
//
// Live refresh is NOT handled here: the owning ExpressionDataFrameTabWidget
// receives the manager signals and re-calls SetObject/SetValue on each tab
// view, so this model stays a passive value renderer.
// =========================================================================

class ExpressionDataFrameModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    /// Rows appended per fetchMore (an integer multiple of xdataset chunks).
    static constexpr int kLoadBatchSize = 256;

    explicit ExpressionDataFrameModel(const xequation::EquationManager &manager,
                                      QObject *parent = nullptr);
    ~ExpressionDataFrameModel() override;

    /// Set the object (equation or registered expression) to display.
    /// The id is resolved through the manager; only REL values
    /// (Measurement / DataArray) display a table, other kinds clear the model.
    void SetObject(const xequation::ObjectId &object_id);

    /// Set a value directly (no Equation/Expression binding).  Only REL
    /// values display a table; other kinds clear the model.  Like SetObject(),
    /// the EquationValue is copied into the model as the DataFrame's stable
    /// owner.
    void SetValue(const xequation::EquationValue &value);

    /// Display the DataFrame view of a Block's tabulated data (its
    /// independent/dependent variables).  A Block has no ObjectId, so this is
    /// the direct entry for Block tree nodes.  The frame is owned and cached
    /// by the Block itself (Block::GetOrCreateDataFrame), so the model only
    /// holds a stable pointer -- no value copy.  The frame is lazily
    /// chunk-loaded, so large blocks are rendered a batch at a time.
    void SetBlock(const xdataset::Block *block);

    /// Clear the model; show no data.
    void Clear();

    bool HasDataFrame() const;

    /// Total DataFrame rows (0 if no data).
    std::size_t total_row_count() const;

    // ---- QAbstractItemModel --------------------------------------------

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(
        int section, Qt::Orientation orientation, int role = Qt::DisplayRole
    ) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

  private:
    /// Stable DataFrame reference of the currently displayed value.  Only valid
    /// when there is a value (equation_value_.HasValue()); otherwise the
    /// call is undefined -- check HasDataFrame() first.
    const xdataset::DataFrame &frame() const;

    /// The held EquationValue (stable owner of the DataFrame; null means no data).
    xequation::EquationValue equation_value_;
    /// Pointer to a Block-cached DataFrame when displaying a Block node
    /// (null otherwise).  The frame is owned by the Block and lives as long
    /// as the dataset is registered in the REL environment.
    const xdataset::DataFrame *block_frame_ = nullptr;
    std::size_t loaded_rows_ = 0;  // rows loaded (exposed to callers)

    /// Manager used to resolve the displayed ObjectId into an Equation / Expression.
    const xequation::EquationManager &manager_;
};

} // namespace gui
} // namespace xresults
