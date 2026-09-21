#pragma once

#include <cstddef>

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

#include <vector>

#include "core/equation_manager.h"
#include "core/equation_value.h"
#include "measurement.h"

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
// DataFrameModel -- presents the DataFrame view of an
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
//   canFetchMore() is true while loaded_rows < DataFrame total rows.
//   Loading happens at two very different sizes:
//     - the FIRST load is kInitialLoadRows (small) -- opening a tab should not
//       pay for a quarter of a big table;
//     - every later fetchMore() is kLoadBatchSize (generous) -- scrolling
//       should not cause a storm of small insertions.
//   Both go through EnsureLoadedRows(), which the view also calls directly to
//   top the initial chunk up to whatever its viewport actually shows.
//
// Live refresh is NOT handled here: the owning DataFrameTabWidget
// receives the manager signals and re-calls SetObject/SetValue on each tab
// view, so this model stays a passive value renderer.
// =========================================================================

class DataFrameModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    /// Rows appended by each fetchMore() once the user scrolls to the bottom.
    /// This is a *scroll* batch, deliberately generous so that dragging the
    /// scrollbar does not trigger a burst of small insertions.
    static constexpr int kLoadBatchSize = 256;

    /// Rows loaded when a table is first attached.
    ///
    /// Deliberately much smaller than kLoadBatchSize: this is the knob for
    /// "how much work does opening a tab cost".  It bounds both the initial
    /// rowsInserted burst and the cell-cache allocation
    /// (rows x columns QStrings -- 32 rows x 66 columns is ~2k, 256 rows is
    /// ~17k).  A viewport shows ~25 rows, so 32 is roughly one screen plus a
    /// little slack; DataFrameView tops this up to actually fill its viewport
    /// (see EnsureViewportFilled) so a taller view still starts complete.
    static constexpr int kInitialLoadRows = 32;

    explicit DataFrameModel(const xequation::EquationManager &manager,
                            QObject *parent = nullptr);
    ~DataFrameModel() override;

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

    // ---- display formatting -------------------------------------------

    /// Replace the display options used for every cell in this model.
    ///
    /// The options are stored locally and passed to
    /// Measurement::to_string(options) per cell, so a model never disturbs the
    /// process-wide FormatDefaults (two open tabs may show different formats).
    /// The cell cache is dropped so the change is visible immediately.
    void SetFormatOptions(const xdataset::FormatOptions &options);
    const xdataset::FormatOptions &format_options() const { return format_options_; }

    /// Total DataFrame rows (0 if no data).
    std::size_t total_row_count() const;

    /// Make sure at least @p rows rows are loaded, clamped to the DataFrame's
    /// total (a no-op when that many are already loaded or there is no data).
    ///
    /// This is the generalisation of fetchMore(): fetchMore() is simply
    /// EnsureLoadedRows(loaded + kLoadBatchSize).  The view calls it directly
    /// to load exactly the rows it needs to fill its viewport, instead of
    /// always pulling in a whole scroll batch.
    void EnsureLoadedRows(std::size_t rows);

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

    // ---- formatting / caching -----------------------------------------

    /// Render one cell.  Each cell is formatted independently through
    /// xdataset::Measurement::to_string(options) with THIS model's options, so
    /// a cell keeps its own auto-scaled unit -- a wide-dynamic-range column
    /// shows "1.5 KV" next to "2 mV" rather than one forced column unit.
    QString FormatCell(int row, int column) const;
    /// Render the multi-index (column 0) of a row.
    static QString FormatMultiIndex(const xdataset::DataFrameRow &row);

    /// Drop every cached cell string (called on reset / format change).
    void InvalidateCellCache();
    /// Grow the cell cache so it can hold @p rows rows.
    void ReserveCellCache(std::size_t rows);

    /// The held EquationValue (stable owner of the DataFrame; null means no data).
    xequation::EquationValue equation_value_;
    /// Pointer to a Block-cached DataFrame when displaying a Block node
    /// (null otherwise).  The frame is owned by the Block and lives as long
    /// as the dataset is registered in the REL environment.
    const xdataset::DataFrame *block_frame_ = nullptr;
    std::size_t loaded_rows_ = 0;  // rows loaded (exposed to callers)

    /// Rendered cell strings, row-major: cell_cache_[row * columns + column].
    /// Qt asks for the same cell many times (size hint, paint, delegate,
    /// selection), and rendering costs a unit lookup + a number conversion,
    /// so results are memoised.  A cell is materialised on first request.
    ///
    /// Mutable: data() is const but fills the cache lazily.
    ///
    /// Memory: ~40 bytes per cell (QString is 4 bytes + shared data), so a
    /// 1000x50 view costs ~2 MB -- acceptable, and it is dropped on reset.
    mutable QVector<QString> cell_cache_;
    /// cell_cache_ validity flags (parallel to cell_cache_).
    mutable QVector<bool> cell_cached_;
    /// Column count cell_cache_ was laid out for (0 = no cache).
    int cache_columns_ = 0;

    /// Display options for THIS model.  Deliberately local rather than global:
    /// two tabs may show the same data in different formats, and a model must
    /// not disturb FormatDefaults for the rest of the application.
    xdataset::FormatOptions format_options_;

    /// Manager used to resolve the displayed ObjectId into an Equation / Expression.
    const xequation::EquationManager &manager_;
};

} // namespace gui
} // namespace xresults
