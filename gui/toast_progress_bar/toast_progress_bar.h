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
#include <QEvent>

namespace xequation
{
namespace gui {

class ToastProgressBar : public QWidget
{
    Q_OBJECT
public:
    explicit ToastProgressBar(const QString &message, QWidget *parent = nullptr);
    ~ToastProgressBar();

    void SetProgress(int value);
    void SetRange(int min, int max);
    void SetMessage(const QString& message);
    void ShowToast(int duration = 3000);
    void Complete();
    void Cancel();
    
    void set_y_offset(int offset)
    {
        y_offset_ = offset;
    }
    int y_offset() const
    {
        return y_offset_;
    }
signals:
    void Canceled();
    void Completed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void SetupUI();
    void SetupConnections();
    void OnCancelClicked();
    void OnCloseButtonClicked();
    void StartFadeOutTimer();
    void MoveToBottomRight();

private:
    QProgressBar *progress_bar_;
    QLabel *title_label_;
    QLabel *message_label_;
    QPushButton *cancel_button_;
    QPushButton *close_button_;
    QTimer *fade_out_timer_;
    QGraphicsOpacityEffect *opacity_effect_;

    int duration_;
    int y_offset_;
    bool is_completed_;
    bool is_canceled_;
};
}
}