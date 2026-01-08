#pragma once

#include "toast_progress_bar.h"
#include <QUuid>
#include <QMap>
#include <QWidget>

namespace xequation
{
namespace gui
{
class ToastManager : public QObject
{
    Q_OBJECT
  public:
    static int kProgressBarMargin;
    ToastManager(QWidget *parent = nullptr);
    ~ToastManager();

    static void SetProgressBarMargin(int margin);

    ToastProgressBar *CreateProgressBar(const QUuid &id, const QString &title, int duration = 1000);

    void UpdateProgressBar(const QUuid &id, int value, const QString &message = "");

    void MakeProgressBarBusy(const QUuid &id, bool is_busy);

    void CompleteProgressBar(const QUuid &id);

    void CancelProgressBar(const QUuid &id);

    ToastProgressBar *GetProgressBar(const QUuid &id) const;
  signals:
    void ProgressCancelRequested(const QUuid &id);

  private:
    void ReorderProgressBars();

    QMap<QUuid, ToastProgressBar *> toast_map_;

    static QMap<QUuid, int> kOccupiedYOffsets;
};
} // namespace gui
} // namespace xequation