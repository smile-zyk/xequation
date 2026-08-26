#include "equation_data_frame_view.h"

#include "equation_data_frame_model.h"

#include <QHeaderView>
#include <QResizeEvent>
#include <QScrollBar>

namespace xresults
{
namespace gui
{

using namespace xequation;

EquationDataFrameView::EquationDataFrameView(QWidget *parent) : QTableView(parent)
{
    SetupUI();
    SetupConnections();
}

EquationDataFrameView::~EquationDataFrameView() = default;

void EquationDataFrameView::SetupUI()
{
    table_model_ = new EquationDataFrameModel(this);
    setModel(table_model_);

    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSortingEnabled(false);

    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setMinimumSectionSize(40);
    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultSectionSize(24);

    // Table size adapts to the window width.
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
}

void EquationDataFrameView::SetupConnections()
{
    connect(
        verticalScrollBar(), &QScrollBar::valueChanged, this,
        &EquationDataFrameView::OnVerticalScrollbarValueChanged
    );
}

void EquationDataFrameView::SetEquation(const Equation *equation)
{
    table_model_->SetEquation(equation);
    // After the model resets, ensure the first screen is loaded (the Qt view
    // does not call fetchMore automatically after setModel/reset; trigger it
    // once explicitly).
    FetchMoreIfNeeded();
}

void EquationDataFrameView::Clear()
{
    table_model_->Clear();
}

void EquationDataFrameView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    FetchMoreIfNeeded();
}

void EquationDataFrameView::showEvent(QShowEvent *event)
{
    QTableView::showEvent(event);
    FetchMoreIfNeeded();
}

void EquationDataFrameView::OnVerticalScrollbarValueChanged(int /*value*/)
{
    FetchMoreIfNeeded();
}

void EquationDataFrameView::FetchMoreIfNeeded()
{
    if (!table_model_)
    {
        return;
    }

    // When scrolled to the bottom (or the first screen is not full), request
    // more rows if the model still has them.  Matches QTreeView's lazy-load
    // pattern: check whether the visible area bottom is the current last row;
    // if so and canFetchMore() is true, call fetchMore().
    const int viewport_height = viewport()->height();
    if (viewport_height <= 0)
    {
        return;
    }

    const QModelIndex bottom_index = indexAt(QPoint(1, viewport_height - 1));
    if (!bottom_index.isValid())
    {
        // No rows in the viewport (first screen not filled or model empty). If
        // the model still has more data, request a batch directly so there is
        // something to show.
        if (table_model_->canFetchMore(QModelIndex()))
        {
            table_model_->fetchMore(QModelIndex());
        }
        return;
    }

    if (bottom_index.row() == table_model_->rowCount() - 1 &&
        table_model_->canFetchMore(QModelIndex()))
    {
        table_model_->fetchMore(QModelIndex());
    }
}

} // namespace gui
} // namespace xresults
