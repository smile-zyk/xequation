#include "value_tree_view.h"
#include "value_tree_model.h"

#include <QEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <limits>
#include <qabstractitemmodel.h>

namespace xequation
{
namespace gui
{
ValueTreeView::ValueTreeView(QWidget *parent) : QTreeView(parent), value_model_(nullptr)
{
    SetupUI();
    SetupConnections();

    header()->viewport()->installEventFilter(this);
}

ValueTreeView::~ValueTreeView() {}

void ValueTreeView::SetValueModel(ValueTreeModel *model)
{
    value_model_ = model;
    QTreeView::setModel(model);
}

void ValueTreeView::SetupUI()
{
    setAlternatingRowColors(true);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(QHeaderView::Interactive);
}

void ValueTreeView::SetupConnections()
{
    connect(header(), &QHeaderView::sectionResized, this, &ValueTreeView::OnHeaderResized);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &ValueTreeView::OnVerticalScrollbarValueChanged);
}

void ValueTreeView::SetHeaderSectionResizeRatio(int idx, double ratio)
{
    header_section_resize_ratios_[idx] = ratio;
}

void ValueTreeView::resizeEvent(QResizeEvent *event)
{
    QTreeView::resizeEvent(event);
    ResizeHeaderSections();
    FetchMoreIfNeeded();
}

void ValueTreeView::showEvent(QShowEvent *event)
{
    QTreeView::showEvent(event);
    ResizeHeaderSections();
}

bool ValueTreeView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == header()->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            header_interactive_dragging_ = true;
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            header_interactive_dragging_ = false;
        }
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void ValueTreeView::ResizeHeaderSections()
{
    int total_width = viewport()->width();

    QMap<int, double> ratios;
    double totalRatio = 0.0;
    for (int i = 0; i < value_model_->columnCount(); ++i)
    {
        ratios[i] = header_section_resize_ratios_.value(i, 1.0);
        totalRatio += ratios[i];
    }

    for (int i = 0; i < ratios.size(); ++i)
    {
        setColumnWidth(i, total_width * ratios[i] / totalRatio);
    }
}

void ValueTreeView::OnHeaderResized(int logical_index, int old_size, int new_size)
{
    if (header_interactive_dragging_ == false)
        return;

    int size_diff = new_size - old_size;
    if (logical_index + 1 < header()->count())
    {
        header()->blockSignals(true);
        int next_section_size = header()->sectionSize(logical_index + 1);
        int min_size = header()->minimumSectionSize();
        if (next_section_size - size_diff > min_size)
        {
            header()->resizeSection(logical_index + 1, next_section_size - size_diff);
        }
        else
        {
            header()->resizeSection(logical_index + 1, min_size);
            header()->resizeSection(logical_index, old_size + next_section_size - min_size);
        }
        header()->blockSignals(false);
    }

    double min_section_size = std::numeric_limits<double>::max();
    QMap<int, double> sizes;

    for (int i = 0; i < value_model_->columnCount(); ++i)
    {
        double section_size = header()->sectionSize(i);
        sizes[i] = section_size;
        if (section_size < min_section_size)
            min_section_size = section_size;
    }

    if (min_section_size > 0)
    {
        for (int i = 0; i < value_model_->columnCount(); ++i)
        {
            header_section_resize_ratios_[i] = sizes[i] / min_section_size;
        }
    }
}

void ValueTreeView::setModel(QAbstractItemModel *model)
{
    QTreeView::setModel(model);
}

void ValueTreeView::OnVerticalScrollbarValueChanged(int value)
{
    FetchMoreIfNeeded();
}

void ValueTreeView::FetchMoreIfNeeded()
{
    int current_height = viewport()->height();
    while (current_height > 0)
    {
        QModelIndex current_index = indexAt(QPoint(0, current_height));
        if (current_index.isValid() == false)
        {
            current_height--;
            continue;
        }
        QModelIndex parent_index = value_model_->parent(current_index);
        if (parent_index.isValid())
        {
            if (current_index.row() == value_model_->rowCount(parent_index) - 1 &&
                value_model_->canFetchMore(parent_index))
            {
                value_model_->fetchMore(parent_index);
                break;
            }
        }
        current_height -= rowHeight(current_index);
    }
}
} // namespace gui
} // namespace xequation