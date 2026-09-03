#include "expression_data_frame_view.h"

#include "expression_data_frame_model.h"

#include <QHeaderView>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollBar>

namespace xresults
{
namespace gui
{

using namespace xequation;

ExpressionDataFrameView::ExpressionDataFrameView(const EquationManager &manager, QWidget *parent)
    : QTableView(parent)
{
    table_model_ = new ExpressionDataFrameModel(manager, this);
    setModel(table_model_);
    SetupUI();
    SetupConnections();
}

ExpressionDataFrameView::~ExpressionDataFrameView() = default;

void ExpressionDataFrameView::SetupUI()
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSortingEnabled(false);

    // Every column sizes to its content; no column is stretched to fill the
    // remaining viewport width.
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setMinimumSectionSize(40);
    verticalHeader()->setVisible(true);
    verticalHeader()->setDefaultSectionSize(24);

    // Table size adapts to the window width.
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);

    // Error overlay: centered on the viewport, replacing the table visually.
    error_label_ = new QLabel(this);
    error_label_->setWordWrap(true);
    error_label_->setAlignment(Qt::AlignCenter);
    error_label_->setStyleSheet(
        QStringLiteral("color: #c0392b; background: rgba(255,255,255,235);")
    );
    error_label_->setVisible(false);
}

void ExpressionDataFrameView::SetupConnections()
{
    connect(
        verticalScrollBar(), &QScrollBar::valueChanged, this,
        &ExpressionDataFrameView::OnVerticalScrollbarValueChanged
    );
}

void ExpressionDataFrameView::SetObject(const ObjectId &object_id)
{
    table_model_->SetObject(object_id);
    // After the model resets, ensure the first screen is loaded (the Qt view
    // does not call fetchMore automatically after setModel/reset; trigger it
    // once explicitly).
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::SetValue(const EquationValue &value)
{
    table_model_->SetValue(value);
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::SetBlock(const xdataset::Block *block)
{
    table_model_->SetBlock(block);
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::Clear()
{
    table_model_->Clear();
}

void ExpressionDataFrameView::SetError(const QString &message)
{
    if (!error_label_)
    {
        return;
    }
    if (message.isEmpty())
    {
        error_label_->clear();
        error_label_->setVisible(false);
        return;
    }
    table_model_->Clear();
    error_label_->setText(message);
    error_label_->setVisible(true);
    CenterErrorLabel();
}

void ExpressionDataFrameView::CenterErrorLabel()
{
    if (!error_label_ || !error_label_->isVisible())
    {
        return;
    }
    // Overlay the viewport area (below headers), leaving a small margin.
    const QRect vp = viewport()->geometry();
    const QMargins margin(8, 8, 8, 8);
    error_label_->setGeometry(
        vp.adjusted(margin.left(), margin.top(), -margin.right(), -margin.bottom())
    );
    error_label_->raise();
}

void ExpressionDataFrameView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    CenterErrorLabel();
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::showEvent(QShowEvent *event)
{
    QTableView::showEvent(event);
    CenterErrorLabel();
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::OnVerticalScrollbarValueChanged(int /*value*/)
{
    FetchMoreIfNeeded();
}

void ExpressionDataFrameView::FetchMoreIfNeeded()
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
