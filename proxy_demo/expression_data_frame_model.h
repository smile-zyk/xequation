#pragma once

#include <cstddef>

#include <QAbstractTableModel>

#include "core/equation_manager.h"
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
// ExpressionDataFrameModel -- presents the DataFrame view of an
// EquationValue(REL) as a two-dimensional table model with Qt's fetchMore
// lazy-loading.
//
// The displayed object is identified by an ObjectId (see
// ExpressionPropertyWidget): either a registered Expression (its Expression::id)
// or an Equation (its Equation::id).
// SetObject() resolves the id through the injected EquationManager& and holds a
// copy of the resulting EquationValue as the DataFrame's stable owner -- because
// rel::Value::data_frame() returns a stable reference owned by the underlying
// DataArray (see REL value.h contract); the caller must keep that Value alive
// while using the frame.  Other types (or a nil id) clear the model.
//
// Lazy loading:
//   rowCount() returns the number of *loaded* rows (not the DataFrame total);
//   canFetchMore() is true while loaded_rows < DataFrame total rows;
//   fetchMore() appends kLoadBatchSize rows at a time, triggered when the
//   QTableView scrolls to the bottom (Qt built-in + view-layer complement).
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
    /// Note: the resolved EquationValue is copied into the model as the
    /// DataFrame's stable owner; the id is kept for event matching.
    void SetObject(const xequation::ObjectId &object_id);

    /// Set a value directly (no Equation/Expression binding; e.g. an
    /// expression watch's eval result).  Only REL values display a table;
    /// other kinds clear the model.  Like SetObject(), the EquationValue is
    /// copied into the model as the DataFrame's stable owner.
    void SetValue(const xequation::EquationValue &value);

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

    /// Receive kExpressionUpdated callbacks (fired on registered-expression
    /// update).  If the updated expression is the one being displayed, the
    /// DataFrame is reloaded.
    void OnExpressionUpdated(const xequation::Expression *expression,
                             bitmask::bitmask<xequation::ExpressionUpdateFlag> flags);

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
    std::size_t loaded_rows_ = 0;  // rows loaded (exposed to callers)

    /// Manager used to resolve the displayed ObjectId into an Equation / Expression.
    const xequation::EquationManager &manager_;

    /// ObjectId of the currently displayed object (nil = no binding).
    xequation::ObjectId object_id_;
};

} // namespace gui
} // namespace xresults
