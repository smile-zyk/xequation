#include "data_frame_table_view.h"

#include "data_frame_table_model.h"

#include <QHeaderView>
#include <QResizeEvent>
#include <QScrollBar>

namespace xequation
{
namespace rel_demo
{

DataFrameTableView::DataFrameTableView(QWidget *parent) : QTableView(parent)
{
    SetupUI();
    SetupConnections();
}

DataFrameTableView::~DataFrameTableView() = default;

void DataFrameTableView::SetupUI()
{
    table_model_ = new DataFrameTableModel(this);
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

    // 表格大小自适应窗口宽度。
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
}

void DataFrameTableView::SetupConnections()
{
    connect(
        verticalScrollBar(), &QScrollBar::valueChanged, this,
        &DataFrameTableView::OnVerticalScrollbarValueChanged
    );
}

void DataFrameTableView::SetEquation(const Equation *equation)
{
    table_model_->SetEquation(equation);
    // 模型重置后，确保首屏数据已加载（Qt 视图不会在 setModel/reset 后
    // 自动调用 fetchMore，这里显式触发一次）。
    FetchMoreIfNeeded();
}

void DataFrameTableView::Clear()
{
    table_model_->Clear();
}

void DataFrameTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    FetchMoreIfNeeded();
}

void DataFrameTableView::showEvent(QShowEvent *event)
{
    QTableView::showEvent(event);
    FetchMoreIfNeeded();
}

void DataFrameTableView::OnVerticalScrollbarValueChanged(int /*value*/)
{
    FetchMoreIfNeeded();
}

void DataFrameTableView::FetchMoreIfNeeded()
{
    if (!table_model_)
    {
        return;
    }

    // 滚动到底部（或首屏装不满）时，若模型还有更多行则请求加载。
    // 与 QTreeView 的懒加载模式一致：检查可视区域底部是否已是
    // 当前最后一行，若是且 canFetchMore() 为真则调用 fetchMore()。
    const int viewport_height = viewport()->height();
    if (viewport_height <= 0)
    {
        return;
    }

    const QModelIndex bottom_index = indexAt(QPoint(1, viewport_height - 1));
    if (!bottom_index.isValid())
    {
        // 视口内尚无行（首屏未填满或模型为空）。若模型仍有更多数据，
        // 直接请求加载一批，确保有内容可显示。
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

} // namespace rel_demo
} // namespace xequation
