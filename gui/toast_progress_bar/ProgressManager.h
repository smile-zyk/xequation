// ProgressManager.h
#ifndef PROGRESSMANAGER_H
#define PROGRESSMANAGER_H

#include <QObject>
#include <QMap>
#include "PopupProgressBar.h"

class ProgressManager : public QObject
{
    Q_OBJECT
    
public:
    static ProgressManager* instance();
    
    // 创建新的进度条
    PopupProgressBar* createProgress(const QString& id, 
                                    const QString& title, 
                                    int duration = 5000);
    
    // 更新进度
    void updateProgress(const QString& id, int value, const QString& message = "");
    
    // 完成进度
    void completeProgress(const QString& id);
    
    // 取消进度
    void cancelProgress(const QString& id);
    
    // 设置范围
    void setProgressRange(const QString& id, int min, int max);
    
private:
    ProgressManager(QObject* parent = nullptr);
    
private:
    QMap<QString, PopupProgressBar*> m_progressBars;
    
private slots:
    void onProgressCancelled();
    void onProgressCompleted();
};

#endif // PROGRESSMANAGER_H