#pragma once

#include "task.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QHashFunctions>
#include <QObject>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

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

    void EnqueueTask(std::unique_ptr<Task> task);

    // 便捷重载：直接传入可调用对象（lambda / 函数指针 / std::function 等）
    // SFINAE 约束：仅当参数能构造成 FuncTask::Callback（即 std::function<void()>）时参与重载，
    // 避免抢走 EnqueueTask(std::unique_ptr<Task>) 的匹配（如 EnqueueTask(std::move(task))）。
    template <typename Callable,
              typename std::enable_if<std::is_constructible<FuncTask::Callback, Callable>::value, int>::type = 0>
    void EnqueueTask(Callable &&callable, const QString &title = QString())
    {
        auto task = std::unique_ptr<FuncTask>(new FuncTask(title, std::forward<Callable>(callable)));
        EnqueueTask(std::move(task));
    }
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
    
    std::deque<QUuid> pending_queue_;
    
    std::unordered_map<QUuid, RunningTaskInfo, QUuidHash> running_tasks_;

    mutable std::mutex mutex_;
};
} // namespace gui
} // namespace xequation