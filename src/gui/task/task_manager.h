#pragma once

#include "task.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QHashFunctions>
#include <QObject>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace xequation
{
namespace gui
{
class TaskManager : public QObject
{
    Q_OBJECT
  public:
    TaskManager(QObject *parent = nullptr, int max_concurrent_tasks = 1);
    ~TaskManager();

    void EnqueueTask(std::unique_ptr<Task> task, int priority = 0);
    void CancelTask(const QUuid &task_id);
    void Shutdown();
    void ClearQueue();

    void SetMaxConcurrentTasks(int max_concurrent_tasks);

    int PendingCount() const;
    int RunningCount() const;
    bool HasPending() const;
    bool IsIdle() const;
    std::vector<QUuid> GetRunningTaskIds() const;
    Task* GetTask(const QUuid &task_id) const;

  signals:
    void TaskQueued(const QUuid &task_id);
    void TaskStarted(const QUuid &task_id);
    void TaskCancelled(const QUuid &task_id);
    void TaskCompleted(const QUuid &task_id);
    void TaskFinished(const QUuid &task_id);
    void TaskProgressUpdated(const QUuid &task_id, int progress, const QString &progress_message);
    void QueueDrained();

  private:
    struct QueuedItem
    {
        QUuid task_id;
        int priority;
        std::size_t enqueue_order{0};
        
        bool operator<(const QueuedItem &other) const
        {
            if (priority == other.priority)
            {
                return enqueue_order > other.enqueue_order;
            }
            return priority < other.priority;
        }
    };

    struct RunningTaskInfo
    {
        std::unique_ptr<QFutureWatcher<void>> watcher;
    };

    struct QUuidHash
    {
        std::size_t operator()(const QUuid &uuid) const noexcept
        {
            return static_cast<std::size_t>(qHash(uuid));
        }
    };

    void MaybeDispatchNext();
    void OnTaskFinished(const QUuid &task_id);
    void ExecuteTask(Task *task);

    int max_concurrent_tasks_{1};
    QThreadPool *thread_pool_{};

    std::unordered_map<QUuid, std::unique_ptr<Task>, QUuidHash> all_tasks_;
    
    std::priority_queue<QueuedItem, std::vector<QueuedItem>> pending_queue_;
    
    std::unordered_map<QUuid, RunningTaskInfo, QUuidHash> running_tasks_;

    std::size_t enqueue_counter_{0};
    mutable std::mutex mutex_;
};
} // namespace gui
} // namespace xequation