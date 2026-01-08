#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QPaintEvent>
#include <QShowEvent>
#include <QCloseEvent>
#include <QEvent>
#include <qpushbutton.h>
#include <QGraphicsDropShadowEffect>

namespace xequation
{
namespace gui {

class ToastProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit ToastProgressBar(const QString &title, int duration, QWidget *parent = nullptr);
    ~ToastProgressBar();

    void SetProgress(int value);
    void SetProgressBusy(bool is_busy);
    void Complete();
    void Cancel();
    void CancelRequest();
    
    void SetYOffset(int offset);
    
    int y_offset() const
    {
        return y_offset_;
    }

    void SetMessage(const QString &message);
    QString GetMessage() const;

signals:
    void CancelRequested();
    void Finished();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void SetupUI();
    void SetupConnections();
    void OnCancelClicked();
    void OnStartFadeOutTimerTimeout();
    void MoveToBottomRight();

private:
    QProgressBar *progress_bar_;
    QLabel *title_label_;
    QLabel *message_label_;
    QPushButton *close_button_;
    QPushButton *cancel_button_;
    QTimer *fade_out_timer_;
    QPropertyAnimation* fade_animation_;
    QGraphicsOpacityEffect *opacity_effect_;

    int duration_;
    int y_offset_;
    bool is_completed_;
    bool is_cancel_requested_;
    bool is_busy_;
};
}
}