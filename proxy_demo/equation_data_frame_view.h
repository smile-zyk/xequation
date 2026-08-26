#pragma once

#include <QTableView>

#include "core/equation.h"

namespace xresults
{
namespace gui
{

class EquationDataFrameModel;

// =========================================================================
// EquationDataFrameView -- a QTableView for displaying a DataFrame table.
//
// SetEquation() passes an Equation* (currently supports only REL values; it is
// turned into a DataFrame table via EquationDataFrameModel) and supports Qt's
// fetchMore lazy loading: requesting the next batch of rows when scrolled to
// the bottom.
// =========================================================================

class EquationDataFrameView : public QTableView
{
    Q_OBJECT
  public:
    explicit EquationDataFrameView(QWidget *parent = nullptr);
    ~EquationDataFrameView() override;

    /// Display the DataFrame view of an Equation.
    /// Supports only REL values (Measurement / DataArray); other types clear
    /// the table.  Note: the Equation is only read during this call and is
    /// forwarded to the model, which converts it to an owned DataFrame; this
    /// class does not hold the Equation pointer.
    void SetEquation(const xequation::Equation *equation);

    /// Clear the table.
    void Clear();

    EquationDataFrameModel *table_model() const { return table_model_; }

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

  private:
    void SetupUI();
    void SetupConnections();
    void OnVerticalScrollbarValueChanged(int value);
    void FetchMoreIfNeeded();

  private:
    EquationDataFrameModel *table_model_ = nullptr;
};

} // namespace gui
} // namespace xresults
