#pragma once

#include "task_manager.h"
#include "toast/toast_manager.h"

namespace xequation
{
namespace gui
{
class ToastTaskManager : public TaskManager
{
    Q_OBJECT
  public:
    ToastTaskManager(QWidget *parent = nullptr, int max_concurrent_tasks = 1);
    ~ToastTaskManager();

  protected:
    void SetupConnections();
    void OnTaskQueued(const QUuid &task_id);
    void OnTaskStarted(const QUuid &task_id);
    void OnTaskProgressUpdated(const QUuid &task_id, int progress, const QString &progress_message);
    void OnTaskCompleted(const QUuid &task_id);
    void OnTaskCancelled(const QUuid &task_id);
    void OnToastProgressCancelRequested(const QUuid &id);

  private:
    ToastManager *toast_manager_;
};
} // namespace gui
} // namespace xequation