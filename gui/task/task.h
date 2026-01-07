#pragma once
#include <QDateTime>
#include <QObject>
#include <QUuid>
#include <atomic>

namespace xequation
{
namespace gui
{
class TaskManager;

class Task : public QObject
{
    Q_OBJECT
  public:
    enum class State
    {
        kPending,
        kRunning,
        kCompleted,
        kCanceling,
        kCancelled
    };
    ~Task() = default;
    virtual void Execute() = 0;
    virtual void Cleanup() = 0;
    virtual void RequestCancel();
    State state() const
    {
        return state_;
    }
    bool IsCompleted() const
    {
        return state_ == State::kCompleted;
    }
    bool IsCancelled() const
    {
        return state_ == State::kCancelled;
    }
    bool IsPending() const
    {
        return state_ == State::kPending;
    }
    bool IsRunning() const
    {
        return state_ == State::kRunning;
    }
    QUuid id() const
    {
        return id_;
    }
    void set_title(const QString &title)
    {
        title_ = title;
    }    
    QString title() const
    {
        return title_;
    }
  signals:
    void Started(QUuid task_id);
    void Completed(QUuid task_id);
    void Cancelled(QUuid task_id);
    void Finished(QUuid task_id);
    void ProgressUpdated(QUuid task_id, int progress, QString progress_message);

  protected:
    Task(const QString& title, QObject *parent = nullptr);
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;
    Task(Task &&) noexcept = delete;
    Task &operator=(Task &&) noexcept = delete;

    void SetProgress(int progress, const QString& message = "");

    QUuid id_;
    QString title_;
    State state_;
    QDateTime create_time_;
    QDateTime start_time_;
    QDateTime end_time_;
    int progress_ = 0;
    QString progress_message_;
    std::atomic<void*> internal_data_ {nullptr};
    std::atomic<bool> cancel_requested_{false};
    friend class TaskManager;
};
} // namespace gui
} // namespace xequation