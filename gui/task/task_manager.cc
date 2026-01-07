#include "task_manager.h"

#include <QDateTime>
#include <QThreadPool>
#include <algorithm>
#include <vector>

namespace xequation
{
namespace gui
{

TaskManager::TaskManager(QObject *parent, int max_concurrent_tasks)
    : QObject(parent), max_concurrent_tasks_(std::max(1, max_concurrent_tasks)), thread_pool_(QThreadPool::globalInstance())
{
}

TaskManager::~TaskManager()
{
    Shutdown();

    std::vector<QFutureWatcher<void> *> watchers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &entry : running_tasks_)
        {
            watchers.push_back(entry.second.watcher.get());
        }
    }

    for (auto *watcher : watchers)
    {
        if (watcher)
        {
            watcher->waitForFinished();
        }
    }
}

void TaskManager::EnqueueTask(std::unique_ptr<Task> task, int priority)
{
    if (task->id_.isNull())
    {
        task->id_ = QUuid::createUuid();
    }
    task->create_time_ = QDateTime::currentDateTime();
    task->state_ = Task::State::kPending;

    QUuid task_id = task->id_;
	auto task_ptr = task.get();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        all_tasks_[task_id] = std::move(task);
        pending_queue_.push(QueuedItem{task_id, priority, enqueue_counter_++});
    }

	connect(task_ptr, &Task::Completed, this, &TaskManager::TaskFinished);
	connect(task_ptr, &Task::Cancelled, this, &TaskManager::TaskFinished);
	connect(task_ptr, &Task::ProgressUpdated, this, &TaskManager::TaskProgressUpdated);
	
    emit TaskQueued(task_id);
    MaybeDispatchNext();
}

void TaskManager::CancelTask(const QUuid &task_id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto running_it = running_tasks_.find(task_id);
    if (running_it != running_tasks_.end())
    {
        auto task_it = all_tasks_.find(task_id);
        if (task_it != all_tasks_.end() && task_it->second)
        {
            task_it->second->state_ = Task::State::kCanceling;
            QtConcurrent::run(thread_pool_, [task_ptr = task_it->second.get()]() {
                task_ptr->RequestCancel();
            });
        }
        return;
    }

    std::vector<QueuedItem> remaining;
    while (!pending_queue_.empty())
    {
        auto queued = pending_queue_.top();
        pending_queue_.pop();
        
        if (queued.task_id == task_id)
        {
            auto task_it = all_tasks_.find(task_id);
            if (task_it != all_tasks_.end() && task_it->second)
            {
                task_it->second->state_ = Task::State::kCancelled;
                task_it->second->Cleanup();
                emit task_it->second->Cancelled(task_id);
                emit TaskCancelled(task_id);
                all_tasks_.erase(task_it);
            }
            continue;
        }
        remaining.push_back(queued);
    }

    for (auto &queued : remaining)
    {
        pending_queue_.push(queued);
    }
}

void TaskManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);

    while (!pending_queue_.empty())
    {
        auto queued = pending_queue_.top();
        pending_queue_.pop();
        
        auto task_it = all_tasks_.find(queued.task_id);
        if (task_it != all_tasks_.end() && task_it->second)
        {
            task_it->second->state_ = Task::State::kCancelled;
            task_it->second->Cleanup();
            emit task_it->second->Cancelled(queued.task_id);
            emit TaskCancelled(queued.task_id);
			all_tasks_.erase(task_it);
        }
    }

    for (auto &entry : running_tasks_)
    {
        auto task_it = all_tasks_.find(entry.first);
        if (task_it != all_tasks_.end() && task_it->second)
        {
            task_it->second->state_ = Task::State::kCanceling;
            QtConcurrent::run(thread_pool_, [task_ptr = task_it->second.get()]() {
                task_ptr->RequestCancel();
            });
        }
    }
}

void TaskManager::ClearQueue()
{
    std::lock_guard<std::mutex> lock(mutex_);

    while (!pending_queue_.empty())
    {
        auto queued = pending_queue_.top();
        pending_queue_.pop();
        
        auto task_it = all_tasks_.find(queued.task_id);
        if (task_it != all_tasks_.end() && task_it->second)
        {
            task_it->second->state_ = Task::State::kCancelled;
            task_it->second->Cleanup();
            emit task_it->second->Cancelled(queued.task_id);
            emit TaskCancelled(queued.task_id);
            all_tasks_.erase(task_it);
        }
    }
}

void TaskManager::SetMaxConcurrentTasks(int max_concurrent_tasks)
{
    max_concurrent_tasks_ = std::max(1, max_concurrent_tasks);
    MaybeDispatchNext();
}

int TaskManager::PendingCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pending_queue_.size());
}

int TaskManager::RunningCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(running_tasks_.size());
}

bool TaskManager::HasPending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_queue_.empty();
}

bool TaskManager::IsIdle() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_queue_.empty() && running_tasks_.empty();
}

std::vector<QUuid> TaskManager::GetRunningTaskIds() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QUuid> ids;
    ids.reserve(running_tasks_.size());
    for (const auto &entry : running_tasks_)
    {
        ids.push_back(entry.first);
    }
    return ids;
}

Task* TaskManager::GetTask(const QUuid &task_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = all_tasks_.find(task_id);
    if (it != all_tasks_.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void TaskManager::ExecuteTask(Task *task)
{
    if (!task)
    {
        return;
    }

    task->state_ = Task::State::kRunning;
    emit task->Started(task->id_);
	emit TaskStarted(task->id_);
    task->start_time_ = QDateTime::currentDateTime();
    task->Execute();
    task->end_time_ = QDateTime::currentDateTime();
}

void TaskManager::MaybeDispatchNext()
{
    while (true)
    {
        QUuid task_id;
        Task *task_ptr = nullptr;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_queue_.empty() ||
                running_tasks_.size() >= static_cast<std::size_t>(max_concurrent_tasks_))
            {
                return;
            }

            auto queued = pending_queue_.top();
            pending_queue_.pop();
            task_id = queued.task_id;

            auto task_it = all_tasks_.find(task_id);
            if (task_it == all_tasks_.end() || !task_it->second)
            {
                continue;
            }
            task_ptr = task_it->second.get();
        }

        if (!task_ptr)
        {
            continue;
        }

        if (task_ptr->id_.isNull())
        {
            task_ptr->id_ = QUuid::createUuid();
        }

        auto watcher = std::make_unique<QFutureWatcher<void>>();
        auto watcher_ptr = watcher.get();

        QObject::connect(watcher_ptr, &QFutureWatcher<QVariant>::finished, this,
                         [this, task_id]() { OnTaskFinished(task_id); });

        auto future = QtConcurrent::run(thread_pool_, [this, task_ptr]() { ExecuteTask(task_ptr); });
        watcher_ptr->setFuture(future);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_tasks_.emplace(task_id, RunningTaskInfo{std::move(watcher)});
        }

        emit TaskStarted(task_id);
    }
}

void TaskManager::OnTaskFinished(const QUuid &task_id)
{
    std::unique_ptr<QFutureWatcher<void>> watcher;
    Task *task_ptr = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto running_it = running_tasks_.find(task_id);
        if (running_it == running_tasks_.end())
        {
            return;
        }

        watcher = std::move(running_it->second.watcher);
        running_tasks_.erase(running_it);

        auto task_it = all_tasks_.find(task_id);
        if (task_it != all_tasks_.end())
        {
            task_ptr = task_it->second.get();
        }
    }

    if (task_ptr)
    {
        if (task_ptr->state_ == Task::State::kCanceling)
        {
            task_ptr->state_ = Task::State::kCancelled;
        }
		if(task_ptr->state_ == Task::State::kRunning)
		{
			task_ptr->state_ = Task::State::kCompleted;
		}
        task_ptr->Cleanup();

        if (task_ptr->state_ == Task::State::kCancelled)
        {
            emit task_ptr->Cancelled(task_id);
            emit TaskCancelled(task_id);
        }
        else
        {
            emit task_ptr->Completed(task_id);
            emit TaskCompleted(task_id);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            all_tasks_.erase(task_id);
        }
    }

    MaybeDispatchNext();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_queue_.empty() && running_tasks_.empty())
        {
            emit QueueDrained();
        }
    }
}

} // namespace gui
} // namespace xequation
