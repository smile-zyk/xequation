#pragma once

#include <QWidget>
#include <QListView>
#include <QStringListModel>
#include <QTimer>

class MessageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MessageWidget(QWidget *parent = nullptr);
    ~MessageWidget() override;

    // Add output message
    void AddMessage(const QString &message);
    
    // Clear all messages
    void Clear();

private slots:
    void OnScrollTimer();

private:
    void FlushPendingMessages();

    QListView *message_list_;
    QStringListModel *model_;
    QTimer scroll_timer_;
    QStringList pending_messages_;
    int pending_count_last_tick_;
};
