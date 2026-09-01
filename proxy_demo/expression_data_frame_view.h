#pragma once

#include <QTableView>

#include "core/equation.h"

class QLabel;

namespace xresults
{
namespace gui
{

class ExpressionDataFrameModel;

// =========================================================================
// ExpressionDataFrameView -- a QTableView for displaying a DataFrame table.
//
// SetEquation() passes an Equation* (currently supports only REL values; it is
// turned into a DataFrame table via ExpressionDataFrameModel) and supports Qt's
// fetchMore lazy loading: requesting the next batch of rows when scrolled to
// the bottom.  Errors are rendered as an overlay label centered on the table
// viewport (SetError), replacing the table content visually.
// =========================================================================

class ExpressionDataFrameView : public QTableView
{
    Q_OBJECT
  public:
    explicit ExpressionDataFrameView(QWidget *parent = nullptr);
    ~ExpressionDataFrameView() override;

    /// Display the DataFrame view of an Equation.
    /// Supports only REL values (Measurement / DataArray); other types clear
    /// the table.  Note: the Equation is only read during this call and is
    /// forwarded to the model, which converts it to an owned DataFrame; this
    /// class does not hold the Equation pointer.
    void SetEquation(const xequation::Equation *equation);

    /// Display a bare value (no Equation binding; e.g. an expression watch's
    /// eval result).  Forwards to the model's SetValue(); only REL values
    /// render a table.
    void SetValue(const xequation::EquationValue &value);

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
