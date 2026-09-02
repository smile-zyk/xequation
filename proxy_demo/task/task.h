#pragma once
#include <QDateTime>
#include <QObject>
#include <QUuid>
#include <atomic>
#include <functional>

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
        return state_.load();
    }
    bool IsCompleted() const
    {
        return state_.load() == State::kCompleted;
    }
    bool IsCancelled() const
    {
        return state_.load() == State::kCancelled;
    }
    bool IsPending() const
    {
        return state_.load() == State::kPending;
    }
    bool IsRunning() const
    {
        return state_.load() == State::kRunning;
    }
    QString error_message() const
    {
        return error_message_;
    }
    bool HasError() const
    {
        return !error_message_.isEmpty();
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
    std::atomic<State> state_{State::kPending};
    QDateTime create_time_;
    QDateTime start_time_;
    QDateTime end_time_;
    int progress_ = 0;
    QString progress_message_;
    QString error_message_;
    std::atomic<bool> cancel_requested_{false};
    friend class TaskManager;
};

// 通用任务：包装任意可调用对象，便于直接 Enqueue 一个 lambda
class FuncTask : public Task
{
    Q_OBJECT
  public:
    using Callback = std::function<void()>;
    // call 为 null 时仅作为跑空任务的占位
    explicit FuncTask(const QString &title, Callback call = nullptr, QObject *parent = nullptr)
        : Task(title, parent), call_(std::move(call))
    {
    }
    ~FuncTask() override = default;

    void Execute() override;
    void Cleanup() override
    {
    }

  private:
    Callback call_;
};
} // namespace gui
} // namespace xequation