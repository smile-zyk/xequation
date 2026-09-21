#pragma once

#include <QStringList>
#include <QTableView>

#include "core/equation_manager.h"
#include "measurement.h"

class QLabel;

namespace xdataset
{
class Block;
}

namespace xresults
{
namespace gui
{

class DataFrameModel;

// =========================================================================
// DataFrameView -- a QTableView for displaying a DataFrame table.
//
// SetObject() passes the ObjectId of an Equation or a registered Expression
// (currently supports only REL values; it is turned into a DataFrame table via
// DataFrameModel) and supports Qt's fetchMore lazy loading:
// requesting the next batch of rows when scrolled to the bottom.  Errors are
// rendered as an overlay label centered on the table viewport (SetError),
// replacing the table content visually.
//
// SetBlock() is the direct entry for Block tree nodes (which have no
// ObjectId): it displays the Block's tabulated frame, lazily chunk-loaded by
// the underlying Block::GetOrCreateDataFrame() cache.
// =========================================================================

class DataFrameView : public QTableView
{
    Q_OBJECT
  public:
    /// Rows loaded beyond the ones the viewport can show, so that a small
    /// scroll (or a slightly taller viewport) does not immediately trigger
    /// another load.
    static constexpr int kViewportOverscanRows = 8;

    explicit DataFrameView(const xequation::EquationManager &manager,
                           QWidget *parent = nullptr);
    ~DataFrameView() override;

    /// Display the DataFrame view of an Equation or registered Expression
    /// (identified by ObjectId).  Supports only REL values (Measurement /
    /// DataArray); other types clear the table.  Note: the object is only read
    /// during this call and is forwarded to the model, which converts its value
    /// to an owned DataFrame; this class does not hold the object pointer.
    void SetObject(const xequation::ObjectId &object_id);

    /// Display a bare value (no Equation/Expression binding; e.g. an expression
    /// watch's eval result).  Forwards to the model's SetValue(); only REL
    /// values render a table.
    void SetValue(const xequation::EquationValue &value);

    /// Display the DataFrame view of a Block's tabulated data (its
    /// independent/dependent variables).  The Block has no ObjectId; the frame
    /// is owned and cached by the Block (lazily chunk-loaded for large data).
    void SetBlock(const xdataset::Block *block);

    /// Clear the table.
    void Clear();

    /// Show an error overlay centered on the table viewport (the table is
    /// cleared underneath); pass an empty message to hide the overlay.
    void SetError(const QString &message);

    /// Replace the display format used for every cell in this view.
    ///
    /// The options are held by the view's model (never the process-wide
    /// xdataset::FormatDefaults), so independent views can show the same data
    /// in different formats.  The columns are re-fitted afterwards because the
    /// rendered text changes width (e.g. "1 KHz" vs "1000 Hz").
    void SetFormatOptions(const xdataset::FormatOptions &options);

    /// The display format currently in use by this view's model.
    const xdataset::FormatOptions &format_options() const;

    DataFrameModel *table_model() const { return table_model_; }

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void SetupUI();
    void SetupConnections();
    void OnVerticalScrollbarValueChanged(int value);
    void FetchMoreIfNeeded();
    /// Load exactly as many rows as the viewport needs (plus
    /// kViewportOverscanRows).  Used when the model's initial load did not
    /// fill the view, so a tall viewport starts complete without pulling in a
    /// whole DataFrameModel::kLoadBatchSize scroll batch.
    void EnsureViewportFilled();
    void CenterErrorLabel();

    /// Fit every column to its content, but only when the header set actually
    /// changed -- a refresh of the same table must not undo a column width the
    /// user dragged.  Runs on every model reset (i.e. on new data).
    void SyncColumnWidths();

  private:
    DataFrameModel *table_model_ = nullptr;
    QLabel *error_label_ = nullptr;
    /// The headers the columns were last fitted for; lets SyncColumnWidths()
    /// tell "a different table" from "the same table, new values".
    QStringList fitted_headers_;
};

} // namespace gui
} // namespace xresults
