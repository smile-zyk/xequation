#include "toast_task_manager.h"

namespace xequation
{
namespace gui
{
ToastTaskManager::ToastTaskManager(QWidget *parent, int max_concurrent_tasks)
    : TaskManager(parent, max_concurrent_tasks)
{
    toast_manager_ = new ToastManager(parent);
    
    SetupConnections();
}

ToastTaskManager::~ToastTaskManager()
{
    delete toast_manager_;
}

void ToastTaskManager::SetupConnections()
{
    connect(this, &TaskManager::TaskQueued, this, &ToastTaskManager::OnTaskQueued);
    connect(this, &TaskManager::TaskStarted, this, &ToastTaskManager::OnTaskStarted);
    connect(this, &TaskManager::TaskProgressUpdated, this, &ToastTaskManager::OnTaskProgressUpdated);
    connect(this, &TaskManager::TaskCompleted, this, &ToastTaskManager::OnTaskCompleted);
    connect(this, &TaskManager::TaskCancelled, this, &ToastTaskManager::OnTaskCancelled);
    connect(toast_manager_, &ToastManager::ProgressCancelRequested, this, &ToastTaskManager::OnToastProgressCancelRequested);
}

void ToastTaskManager::OnTaskQueued(const QUuid &task_id)
{
    Task* task = GetTask(task_id);
    if(task == nullptr)
    {
        return;
    }

    toast_manager_->CreateProgressBar(task_id, task->title());
    toast_manager_->UpdateProgressBar(task_id, -1, "Task Queued");
}

void ToastTaskManager::OnTaskStarted(const QUuid &task_id) 
{
    Task* task = GetTask(task_id);
    if(task == nullptr)
    {
        return;
    }
}

void ToastTaskManager::OnTaskProgressUpdated(const QUuid &task_id, int progress, const QString &progress_message) 
{
    toast_manager_->UpdateProgressBar(task_id, progress, progress_message);
}

void ToastTaskManager::OnTaskCompleted(const QUuid &task_id) 
{
    toast_manager_->CompleteProgressBar(task_id);
}

void ToastTaskManager::OnTaskCancelled(const QUuid &task_id) 
{
    toast_manager_->CancelProgressBar(task_id);
}

void ToastTaskManager::OnToastProgressCancelRequested(const QUuid &id) 
{
    CancelTask(id);
}
} // namespace gui
} // namespace xequation