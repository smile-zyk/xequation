#include "toast_manager.h"
#include "toast_progress_bar.h"

namespace xequation
{
namespace gui
{

int ToastManager::kProgressBarMargin = 10;
QMap<QUuid, int> ToastManager::kOccupiedYOffsets;

ToastManager::ToastManager(QWidget *parent)
    : QObject(parent)
{
}

ToastManager::~ToastManager()
{
    for (auto it = toast_map_.begin(); it != toast_map_.end(); ++it)
    {
        if (it.value())
        {
            it.value()->deleteLater();
        }
    }
    toast_map_.clear();
    kOccupiedYOffsets.clear();
}

void ToastManager::SetProgressBarMargin(int margin)
{
    kProgressBarMargin = margin;
}

ToastProgressBar *ToastManager::CreateProgressBar(const QUuid &id, const QString &title, int duration)
{
    if (toast_map_.contains(id))
    {
        return nullptr;
    }

    ToastProgressBar *toast = new ToastProgressBar(title, duration, qobject_cast<QWidget*>(parent()));
    int y_offset = 0;
    for(const auto &offset : kOccupiedYOffsets)
    {
        if (offset >= y_offset)
        {
            y_offset = offset + toast->height() + kProgressBarMargin;
        }
    }
    
    toast->SetYOffset(y_offset);
    kOccupiedYOffsets[id] = y_offset;
    toast_map_[id] = toast;

    connect(toast, &ToastProgressBar::CancelRequested, this, [this, id]() {
        emit ProgressCancelRequested(id);
    });

    connect(toast, &ToastProgressBar::Finished, this, [this, id]() {
        if (toast_map_.contains(id))
        {
            kOccupiedYOffsets.remove(id);
            toast_map_[id]->deleteLater();
            toast_map_.remove(id);
            ReorderProgressBars();
        }
    });

    toast->show();
    return toast;
}

void ToastManager::UpdateProgressBar(const QUuid &id, int value, const QString &message)
{
    if (!toast_map_.contains(id))
    {
        return;
    }

    ToastProgressBar *toast = toast_map_[id];

    if(value == -1)
    {
        toast->SetProgressBusy(true);
    }
    else 
    {
        toast->SetProgress(value);
    }

    if (!message.isEmpty())
    {
        toast->SetMessage(message);
    }
}

void ToastManager::MakeProgressBarBusy(const QUuid &id, bool is_busy)
{
    if (!toast_map_.contains(id))
    {
        return;
    }

    ToastProgressBar *toast = toast_map_[id];
    toast->SetProgressBusy(is_busy);
}

void ToastManager::CompleteProgressBar(const QUuid &id)
{
    if (!toast_map_.contains(id))
    {
        return;
    }

    ToastProgressBar *toast = toast_map_[id];
    toast->Complete();
}

void ToastManager::CancelProgressBar(const QUuid &id)
{
    if (!toast_map_.contains(id))
    {
        return;
    }

    ToastProgressBar *toast = toast_map_[id];
    toast->Cancel();
}

ToastProgressBar *ToastManager::GetProgressBar(const QUuid &id) const
{
    if (toast_map_.contains(id))
    {
        return toast_map_.value(id);
    }
    return nullptr;
}

void ToastManager::ReorderProgressBars()
{
    QMap<int, QUuid> sorted_offsets;
    for (auto it = kOccupiedYOffsets.begin(); it != kOccupiedYOffsets.end(); ++it)
    {
        sorted_offsets[it.value()] = it.key();
    }

    int new_y_offset = 0;
    for (auto it = sorted_offsets.begin(); it != sorted_offsets.end(); ++it)
    {
        QUuid uuid = it.value();
        kOccupiedYOffsets[uuid] = new_y_offset;
        ToastProgressBar *toast = nullptr;
        if (toast_map_.contains(uuid))
        {
            toast = toast_map_[uuid];
            toast->SetYOffset(new_y_offset);
        }
        
        new_y_offset += toast->height() + kProgressBarMargin;
    }
}
} // namespace gui
} // namespace xequation