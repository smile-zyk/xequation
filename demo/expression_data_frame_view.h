#pragma once

#include <QTableView>

#include "core/equation_manager.h"

class QLabel;

namespace xdataset
{
class Block;
}

namespace xresults
{
namespace gui
{

class ExpressionDataFrameModel;

// =========================================================================
// ExpressionDataFrameView -- a QTableView for displaying a DataFrame table.
//
// SetObject() passes the ObjectId of an Equation or a registered Expression
// (currently supports only REL values; it is turned into a DataFrame table via
// ExpressionDataFrameModel) and supports Qt's fetchMore lazy loading:
// requesting the next batch of rows when scrolled to the bottom.  Errors are
// rendered as an overlay label centered on the table viewport (SetError),
// replacing the table content visually.
//
// SetBlock() is the direct entry for Block tree nodes (which have no
// ObjectId): it displays the Block's tabulated frame, lazily chunk-loaded by
// the underlying Block::GetOrCreateDataFrame() cache.
// =========================================================================

class ExpressionDataFrameView : public QTableView
{
    Q_OBJECT
  public:
    explicit ExpressionDataFrameView(const xequation::EquationManager &manager,
                                     QWidget *parent = nullptr);
    ~ExpressionDataFrameView() override;

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

    ExpressionDataFrameModel *table_model() const { return table_model_; }

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void SetupUI();
    void SetupConnections();
    void OnVerticalScrollbarValueChanged(int value);
    void FetchMoreIfNeeded();
    void CenterErrorLabel();

  private:
    ExpressionDataFrameModel *table_model_ = nullptr;
    QLabel *error_label_ = nullptr;
};

} // namespace gui
} // namespace xresults
