#include "value_tree_view.h"

#include <QHeaderView>
#include <QEvent>
#include <limits>

namespace xequation
{
namespace gui
{

ValueTreeView::ValueTreeView(QWidget *parent) 
    : QTreeView(parent), value_model_(new ValueTreeModel(this))
{
    SetupUI();
    SetupConnections();

    header()->viewport()->installEventFilter(this);
}

ValueTreeView::~ValueTreeView() {}

void ValueTreeView::SetupUI()
{
    setModel(value_model_.get());

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(QHeaderView::Interactive);
}

void ValueTreeView::SetupConnections()
{
    connect(header(), &QHeaderView::sectionResized, this, &ValueTreeView::OnHeaderResized);
}

ValueTreeModel *ValueTreeView::value_model() const
{
    return value_model_.get();
}

void ValueTreeView::SetHeaderSectionResizeRatio(int idx, double ratio)
{
    header_section_resize_ratios_[idx] = ratio;
}

void ValueTreeView::resizeEvent(QResizeEvent *event)
{
    QTreeView::resizeEvent(event);
    ResizeHeaderSections();
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

} // namespace gui
} // namespace xequation