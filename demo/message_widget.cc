#include "message_widget.h"
#include <QVBoxLayout>
#include <QLabel>

MessageWidget::MessageWidget(QWidget *parent)
    : QWidget(parent), pending_count_last_tick_(0)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Add title
    auto title_label = new QLabel("Python Output");
    layout->addWidget(title_label);
    
    // Create model and view
    model_ = new QStringListModel(this);
    message_list_ = new QListView();
    message_list_->setModel(model_);
    layout->addWidget(message_list_);
    
    setLayout(layout);
    
    // Setup timer for batch message processing
    scroll_timer_.setInterval(50);  // 50ms timer
    scroll_timer_.start();  // Always running
    connect(&scroll_timer_, &QTimer::timeout, this, &MessageWidget::OnScrollTimer);
}

MessageWidget::~MessageWidget()
{
}

void MessageWidget::AddMessage(const QString &message)
{
    // Only append to buffer, don't update model directly
    pending_messages_.append(message);
}

void MessageWidget::Clear()
{
    model_->setStringList(QStringList());
    pending_messages_.clear();
    pending_count_last_tick_ = 0;
}

void MessageWidget::OnScrollTimer()
{
    int pending_count = pending_messages_.size();
    
    if (pending_count == 0) {
        pending_count_last_tick_ = 0;
        return;
    }
    
    // Flush conditions: >=50 messages OR has pending messages for 2 consecutive ticks
    if (pending_count >= 50 || pending_count_last_tick_ > 0) {
        FlushPendingMessages();
        pending_count_last_tick_ = 0;
    } else {
        // Mark that we have messages, will force flush on next tick
        pending_count_last_tick_ = pending_count;
    }
}

void MessageWidget::FlushPendingMessages()
{
    if (pending_messages_.isEmpty()) {
        return;
    }
    
    // 禁用视图更新
    message_list_->setUpdatesEnabled(false);
    
    int start_row = model_->rowCount();
    model_->insertRows(start_row, pending_messages_.size());
    
    for (int i = 0; i < pending_messages_.size(); ++i) {
        model_->setData(model_->index(start_row + i), pending_messages_[i]);
    }
    
    // 重新启用并刷新
    message_list_->setUpdatesEnabled(true);
    
    // 滚动到底部
    int row_count = model_->rowCount();
    if (row_count > 0) {
        message_list_->scrollTo(model_->index(row_count - 1));
    }
    
    pending_messages_.clear();
}
