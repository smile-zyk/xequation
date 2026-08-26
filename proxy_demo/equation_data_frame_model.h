#pragma once

#include <cstddef>
#include <string>

#include <QAbstractTableModel>

#include "core/equation.h"
#include "core/equation_signals_manager.h"
#include "core/equation_value.h"

namespace xdataset
{
class DataFrame;
}

namespace xresults
{
namespace gui
{

// =========================================================================
// EquationDataFrameModel -- presents the DataFrame view of an
// EquationValue(REL) as a two-dimensional table model with Qt's fetchMore
// lazy-loading.
//
// Data source:
//   - SetEquation(): supports only REL values (Measurement / DataArray).
//     It calls Equation::GetValue() to obtain an EquationValue and holds a
//     copy of it as the DataFrame's stable owner -- because
//     rel::Value::data_frame() returns a stable reference owned by the
//     underlying DataArray (see REL value.h contract); the caller must keep
//     that Value alive while using the frame.  Other types (or a null
//     pointer) clear the model.
//
// Lazy loading:
//   rowCount() returns the number of *loaded* rows (not the DataFrame total);
//   canFetchMore() is true while loaded_rows < DataFrame total rows;
//   fetchMore() appends kLoadBatchSize rows at a time, triggered when the
//   QTableView scrolls to the bottom (Qt built-in + view-layer complement).
// =========================================================================

class EquationDataFrameModel : public QAbstractTableModel
{
    Q_OBJECT
  public:
    /// Rows appended per fetchMore (an integer multiple of xdataset chunks).
    static constexpr int kLoadBatchSize = 256;

    explicit EquationDataFrameModel(QObject *parent = nullptr);
    ~EquationDataFrameModel() override;

    /// Set the Equation to display.
    /// Supports only REL values (Measurement / DataArray); other types clear
    /// the model.  Note: copies Equation::GetValue()'s result and holds it as
    /// the DataFrame owner; does not hold the Equation pointer.
    void SetEquation(const xequation::Equation *equation);

    /// Clear the model; show no data.
    void Clear();

    bool HasDataFrame() const;

    /// Total DataFrame rows (0 if no data).
    std::size_t total_row_count() const;

    /// Receive kEquationRemoving callbacks (fired before deletion).  External
    /// code connects both engines' managers to this (the model does not manage
    /// connections).  If the removed equation is the one being displayed, the
    /// model is cleared.
    void OnEquationRemoving(const xequation::Equation *equation);

    /// Receive kEquationUpdated callbacks (fired on equation update).  External
    /// code connects both engines' managers to this (the model does not manage
    /// connections).  If the updated equation is the one being displayed, the
    /// DataFrame is reloaded.
    void OnEquationUpdated(const xequation::Equation *equation,
                           bitmask::bitmask<xequation::EquationUpdateFlag> flags);

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
    /// when there is a REL value (equation_value_.IsRelValue()); otherwise the
    /// call is undefined -- check HasDataFrame() first.
    const xdataset::DataFrame &frame() const;

    /// The held EquationValue (stable owner of the DataFrame; null means no data).
    xequation::EquationValue equation_value_;
    std::size_t loaded_rows_ = 0;  // rows loaded (exposed to callers)

    /// Name of the currently displayed equation (compared in OnEquationRemoving).
    std::string equation_name_;
};

} // namespace gui
} // namespace xresults
