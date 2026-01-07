#include "task.h"

namespace xequation
{
namespace gui
{

Task::Task(const QString& title, QObject *parent)
    : QObject(parent), state_(State::kPending), title_(title) 
{
    connect(this, &Task::Completed, this, &Task::Finished);
    connect(this, &Task::Cancelled, this, &Task::Finished);
}

void Task::RequestCancel()
{
    cancel_requested_.store(true);
}

void Task::SetProgress(int progress, const QString &message)
{
    if (progress < 0)
    {
        progress_ = 0;
    }
    else if (progress > 100)
    {
        progress_ = 100;
    }
    else
    {
        progress_ = progress;
    }
    progress_message_ = message;
    emit ProgressUpdated(id_, progress_, progress_message_);
}

} // namespace gui
} // namespace xequation
