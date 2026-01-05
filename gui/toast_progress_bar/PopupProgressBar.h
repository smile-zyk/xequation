// PopupProgressBar.h
#ifndef POPUPPROGRESSBAR_H
#define POPUPPROGRESSBAR_H

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

class PopupProgressBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float opacity READ opacity WRITE setOpacity)
    
public:
    explicit PopupProgressBar(const QString& title, 
                            int duration = 5000,  // 默认5秒后开始淡出
                            QWidget* parent = nullptr);
    ~PopupProgressBar();
    
    void setProgress(int value);
    void setRange(int min, int max);
    void setMessage(const QString& message);
    void complete();
    void cancel();
    float opacity() const;
    void setOpacity(float opacity);
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    static int getNextYOffset();
    static void releaseYOffset(int offset);
    
signals:
    void cancelled();
    void completed();
    
private slots:
    void onCancelClicked();
    void onCloseButtonClicked();
    void startFadeOut();
    
private:
    void setupUI();
    void startAutoCloseTimer();
    void moveToBottomRight();
    
private:
    QProgressBar* m_progressBar;
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QPushButton* m_cancelButton;
    QPushButton* m_closeButton;
    QTimer* m_fadeTimer;
    QTimer* m_closeTimer;
    QGraphicsOpacityEffect* m_opacityEffect;
    int m_duration;
    int m_yOffset;
    bool m_isCompleted;
    bool m_isCancelled;
    
    static QList<int> s_occupiedOffsets;
    static const int WINDOW_WIDTH = 300;
    static const int WINDOW_HEIGHT = 100;
    static const int MARGIN = 20;
};

#endif // POPUPPROGRESSBAR_H