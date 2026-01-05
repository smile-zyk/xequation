// ProgressManager.cpp
#include "ProgressManager.h"
#include <QApplication>

ProgressManager* ProgressManager::instance()
{
    static ProgressManager manager;
    return &manager;
}

ProgressManager::ProgressManager(QObject* parent)
    : QObject(parent)
{
}

PopupProgressBar* ProgressManager::createProgress(const QString& id, 
                                                const QString& title, 
                                                int duration)
{
    // 如果已存在，先移除旧的
    if (m_progressBars.contains(id)) {
        PopupProgressBar* oldBar = m_progressBars.value(id);
        if (oldBar) {
            oldBar->close();
        }
        m_progressBars.remove(id);
    }
    
    // 创建新的进度条，绑定到当前活动窗口以便定位到窗口右下角
    QWidget* parentWidget = QApplication::activeWindow();
    PopupProgressBar* progressBar = new PopupProgressBar(title, duration, parentWidget);
    m_progressBars.insert(id, progressBar);
    
    // 连接信号
    connect(progressBar, &PopupProgressBar::cancelled, this, &ProgressManager::onProgressCancelled);
    connect(progressBar, &PopupProgressBar::completed, this, &ProgressManager::onProgressCompleted);
    // 确保窗口关闭后从管理表移除，避免悬空指针
    connect(progressBar, &QObject::destroyed, this, [this, id]() {
        m_progressBars.remove(id);
    });
    
    progressBar->show();
    return progressBar;
}

void ProgressManager::updateProgress(const QString& id, int value, const QString& message)
{
    if (m_progressBars.contains(id)) {
        PopupProgressBar* progressBar = m_progressBars.value(id);
        if (progressBar) {
            progressBar->setProgress(value);
            if (!message.isEmpty()) {
                progressBar->setMessage(message);
            }
        }
    }
}

void ProgressManager::completeProgress(const QString& id)
{
    if (m_progressBars.contains(id)) {
        PopupProgressBar* progressBar = m_progressBars.value(id);
        if (progressBar) {
            progressBar->complete();
        }
    }
}

void ProgressManager::cancelProgress(const QString& id)
{
    if (m_progressBars.contains(id)) {
        PopupProgressBar* progressBar = m_progressBars.value(id);
        if (progressBar) {
            progressBar->cancel();
        }
    }
}

void ProgressManager::setProgressRange(const QString& id, int min, int max)
{
    if (m_progressBars.contains(id)) {
        PopupProgressBar* progressBar = m_progressBars.value(id);
        if (progressBar) {
            progressBar->setRange(min, max);
        }
    }
}

void ProgressManager::onProgressCancelled()
{
    PopupProgressBar* senderBar = qobject_cast<PopupProgressBar*>(sender());
    if (senderBar) {
        QString id = m_progressBars.key(senderBar);
        if (!id.isEmpty()) {
            m_progressBars.remove(id);
        }
    }
}

void ProgressManager::onProgressCompleted()
{
    PopupProgressBar* senderBar = qobject_cast<PopupProgressBar*>(sender());
    if (senderBar) {
        QString id = m_progressBars.key(senderBar);
        if (!id.isEmpty()) {
            // 完成后不立即移除，等待窗口关闭
        }
    }
}