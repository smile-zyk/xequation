#include "data_frame_view.h"

#include "data_frame_model.h"

#include <QHeaderView>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollBar>

namespace xresults
{
namespace gui
{

using namespace xequation;

DataFrameView::DataFrameView(const EquationManager &manager, QWidget *parent)
    : QTableView(parent)
{
    table_model_ = new DataFrameModel(manager, this);
    setModel(table_model_);
    SetupUI();
    SetupConnections();
}

DataFrameView::~DataFrameView() = default;

void DataFrameView::SetupUI()
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSortingEnabled(false);

    // Every column sizes to its content on load; no column is stretched to
    // fill the remaining viewport width.
    horizontalHeader()->setStretchLastSection(false);
    // Interactive (not ResizeToContents) so the user can drag a section
    // divider to resize a column, and double-click a divider to auto-fit that
    // one column.  The initial widths are fitted to the content by
    // SyncColumnWidths() once data arrives -- ResizeToContents as the section
    // mode would re-fit on every single change and fight the user's drag.
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setSectionsClickable(true);
    horizontalHeader()->setHighlightSections(false);
    // resizeSections(ResizeToContents) measures a column by asking data() for
    // EVERY loaded row (up to resizeContentsPrecision, default 1000).  With
    // many columns that is O(rows x columns) renders -- e.g. 50 columns x 1000
    // rows = 50k Measurement::to_string() calls for a single header layout.
    // Sampling the first 32 rows is visually indistinguishable and cuts it by
    // ~30x.
    horizontalHeader()->setResizeContentsPrecision(32);
    horizontalHeader()->setMinimumSectionSize(40);
    horizontalHeader()->setDefaultSectionSize(90);
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

void DataFrameView::SetupConnections()
{
    connect(
        verticalScrollBar(), &QScrollBar::valueChanged, this,
        &DataFrameView::OnVerticalScrollbarValueChanged
    );
    // A reset means "new table / new values": re-fit the columns when the
    // header set changed (see SyncColumnWidths).
    connect(
        table_model_, &QAbstractItemModel::modelReset, this,
        &DataFrameView::SyncColumnWidths
    );
}

void DataFrameView::SyncColumnWidths()
{
    if (!table_model_)
    {
        return;
    }

    const int columns = table_model_->columnCount();
    if (columns <= 0)
    {
        fitted_headers_.clear();
        return;
    }

    // Collect the current column headers.  headerData() for a column header is
    // a plain string read from the frame's header list, so this is cheap even
    // for a 66-column S-parameter table (unlike the cell data() it avoids).
    QStringList headers;
    headers.reserve(columns);
    for (int column = 0; column < columns; ++column)
    {
        headers.append(
            table_model_->headerData(column, Qt::Horizontal, Qt::DisplayRole)
                .toString()
        );
    }

    // Same table, new values -> keep whatever widths the user chose.
    if (headers == fitted_headers_)
    {
        return;
    }
    fitted_headers_ = headers;
    horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}

void DataFrameView::SetObject(const ObjectId &object_id)
{
    table_model_->SetObject(object_id);
    // After the model resets, ensure the first screen is loaded (the Qt view
    // does not call fetchMore automatically after setModel/reset; trigger it
    // once explicitly).
    FetchMoreIfNeeded();
}

void DataFrameView::SetValue(const EquationValue &value)
{
    table_model_->SetValue(value);
    FetchMoreIfNeeded();
}

void DataFrameView::SetBlock(const xdataset::Block *block)
{
    table_model_->SetBlock(block);
    FetchMoreIfNeeded();
}

void DataFrameView::Clear()
{
    table_model_->Clear();
}

void DataFrameView::SetError(const QString &message)
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

void DataFrameView::SetFormatOptions(const xdataset::FormatOptions &options)
{
    table_model_->SetFormatOptions(options);
    // The rendered text changed width ("1 KHz" vs "1000 Hz"), so force a
    // re-fit by dropping the cached header signature first.
    fitted_headers_.clear();
    SyncColumnWidths();
}

const xdataset::FormatOptions &DataFrameView::format_options() const
{
    return table_model_->format_options();
}

void DataFrameView::CenterErrorLabel()
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

void DataFrameView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    CenterErrorLabel();
    FetchMoreIfNeeded();
}

void DataFrameView::showEvent(QShowEvent *event)
{
    QTableView::showEvent(event);
    CenterErrorLabel();
    FetchMoreIfNeeded();
}

void DataFrameView::OnVerticalScrollbarValueChanged(int /*value*/)
{
    FetchMoreIfNeeded();
}

void DataFrameView::EnsureViewportFilled()
{
    if (!table_model_)
    {
        return;
    }

    // Rows that fit in the viewport, rounded up.  Row heights are uniform
    // (verticalHeader()->setDefaultSectionSize in SetupUI), so one section
    // size is representative.
    const int row_height = verticalHeader()->defaultSectionSize();
    const int viewport_height = viewport()->height();
    if (row_height <= 0 || viewport_height <= 0)
    {
        return;
    }
    const int visible_rows = viewport_height / row_height + 1;

    table_model_->EnsureLoadedRows(
        static_cast<std::size_t>(visible_rows + kViewportOverscanRows));
}

void DataFrameView::FetchMoreIfNeeded()
{
    if (!table_model_)
    {
        return;
    }

    // When scrolled to the bottom (or the first screen is not full), request
    // more rows if the model still has them.  Matches QTreeView's lazy-load
    // pattern: check whether the visible area bottom is the current last row;
    // if so and canFetchMore() is true, request more.
    const int viewport_height = viewport()->height();
    if (viewport_height <= 0)
    {
        return;
    }

    const QModelIndex bottom_index = indexAt(QPoint(1, viewport_height - 1));
    if (!bottom_index.isValid())
    {
        // The first screen is not filled (or the model is empty).  Load just
        // enough rows to fill the viewport rather than a whole scroll batch:
        // the model's initial load (DataFrameModel::kInitialLoadRows) is
        // deliberately small, so a tall viewport has to top it up, and asking
        // for a 256-row batch here would defeat that.
        EnsureViewportFilled();
        return;
    }

    if (bottom_index.row() == table_model_->rowCount() - 1 &&
        table_model_->canFetchMore(QModelIndex()))
    {
        // The user reached the bottom: pull in a generous scroll batch so
        // dragging the scrollbar does not cause a storm of small insertions.
        table_model_->fetchMore(QModelIndex());
    }
}

} // namespace gui
} // namespace xresults
