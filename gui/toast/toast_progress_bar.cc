#include "toast_progress_bar.h"
#include "toast_manager.h"

#include <QPainter>
#include <QPainterPath>

namespace xequation
{
namespace gui
{

ToastProgressBar::ToastProgressBar(const QString &title, int duration, QWidget *parent)
    : QWidget(parent), duration_(duration), y_offset_(0), is_completed_(false), is_cancel_requested_(false)
{
    setWindowTitle(title);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(500);
    SetupUI();
    SetupConnections();
}

ToastProgressBar::~ToastProgressBar() {}

void ToastProgressBar::SetProgress(int value)
{
    if (is_busy_)
    {
        is_busy_ = false;
        progress_bar_->setRange(0, 100); // Switch to determinate
    }
    progress_bar_->setValue(value);
}

void ToastProgressBar::SetProgressBusy(bool is_busy)
{
    is_busy_ = is_busy;
    if (is_busy_)
    {
        progress_bar_->setRange(0, 0); // Indeterminate
    }
    else
    {
        progress_bar_->setRange(0, 100); // Determinate
    }
}

void ToastProgressBar::SetMessage(const QString &message)
{
    message_label_->setText(message);
}

QString ToastProgressBar::GetMessage() const
{
    return message_label_->text();
}

void ToastProgressBar::Complete()
{
    if (is_completed_ || is_cancel_requested_)
        return;

    is_completed_ = true;
    cancel_button_->setEnabled(false);

    fade_out_timer_->start(duration_);
}

void ToastProgressBar::CancelRequest()
{
    if (is_cancel_requested_ || is_completed_)
        return;

    is_cancel_requested_ = true;
    cancel_button_->setEnabled(false);

    emit CancelRequested();
}

void ToastProgressBar::Cancel()
{
    if (is_cancel_requested_ == false)
    {
        return;
    }

    fade_out_timer_->start(duration_);
}

void ToastProgressBar::SetYOffset(int offset)
{
    y_offset_ = offset;
    MoveToBottomRight();
}

void ToastProgressBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    MoveToBottomRight();
}

void ToastProgressBar::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
    emit Finished();
}

void ToastProgressBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw rounded rectangle background with shadow
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    
    // Draw shadow with increased opacity and blur
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 15; ++i)
    {
        int alpha = 30 - i * 2;
        if (alpha < 0) alpha = 0;
        QColor shadowColor(0, 0, 0, alpha);
        painter.setBrush(shadowColor);
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(rect().adjusted(-i, -i + 5, i, i + 5), 10 + i, 10 + i);
        painter.drawPath(shadowPath);
    }
    
    // Draw white background
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawPath(path);
    
    QWidget::paintEvent(event);
}

bool ToastProgressBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget())
    {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize)
        {
            MoveToBottomRight();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ToastProgressBar::SetupUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(20, 15, 20, 15);
    main_layout->setSpacing(12);

    QHBoxLayout *title_layout = new QHBoxLayout();
    title_label_ = new QLabel(windowTitle(), this);
    QFont title_font = title_label_->font();
    title_font.setPointSize(9);
    title_font.setBold(true);
    title_label_->setFont(title_font);
    title_layout->addWidget(title_label_);
    title_layout->addStretch();
    close_button_ = new QPushButton(this);
    close_button_->setIcon(QIcon(":/icons/close.png"));
    close_button_->setFlat(true);
    close_button_->setFixedSize(24, 24);
    close_button_->setCursor(Qt::PointingHandCursor);
    title_layout->addWidget(close_button_);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(true);
    progress_bar_->setFormat("%p%");
    progress_bar_->setMinimumHeight(20);
    QFont progress_font = progress_bar_->font();
    progress_font.setPointSize(8);
    progress_bar_->setFont(progress_font);

    message_label_ = new QLabel(this);
    message_label_->setWordWrap(true);
    QFont message_font = message_label_->font();
    message_font.setPointSize(8);
    message_label_->setFont(message_font);

    QHBoxLayout *button_layout = new QHBoxLayout();
    button_layout->addStretch();

    cancel_button_ = new QPushButton("Cancel", this);
    cancel_button_->setMinimumWidth(80);
    cancel_button_->setMinimumHeight(28);
    cancel_button_->setCursor(Qt::PointingHandCursor);
    QFont button_font = cancel_button_->font();
    button_font.setPointSize(8);
    cancel_button_->setFont(button_font);

    button_layout->addWidget(cancel_button_);

    main_layout->addLayout(title_layout);
    main_layout->addWidget(progress_bar_);
    main_layout->addWidget(message_label_);
    main_layout->addLayout(button_layout);

    fade_out_timer_ = new QTimer(this);
    fade_out_timer_->setSingleShot(true);

    opacity_effect_ = new QGraphicsOpacityEffect(this);
    opacity_effect_->setOpacity(1.0);
    setGraphicsEffect(opacity_effect_);

    fade_animation_ = new QPropertyAnimation(opacity_effect_, "opacity");
    adjustSize();
}

void ToastProgressBar::SetupConnections()
{
    connect(cancel_button_, &QPushButton::clicked, this, &ToastProgressBar::OnCancelClicked);
    connect(close_button_, &QPushButton::clicked, this, &ToastProgressBar::close);
    connect(fade_out_timer_, &QTimer::timeout, this, &ToastProgressBar::OnStartFadeOutTimerTimeout);
    connect(fade_animation_, &QPropertyAnimation::finished, this, &QWidget::close);

    if (parentWidget())
    {
        parentWidget()->installEventFilter(this);
    }
}

void ToastProgressBar::OnCancelClicked()
{
    CancelRequest();
}

void ToastProgressBar::OnStartFadeOutTimerTimeout()
{
    if (!fade_out_timer_->isActive())
    {
        fade_out_timer_->stop();
    }

    fade_animation_->setDuration(1000);
    fade_animation_->setStartValue(opacity_effect_->opacity());
    fade_animation_->setEndValue(0.0);
    fade_animation_->setEasingCurve(QEasingCurve::OutCubic);
    fade_animation_->start();
}

void ToastProgressBar::MoveToBottomRight()
{
    if (parentWidget())
    {
        QRect parentRect = parentWidget()->geometry();
        int x = parentRect.right() - width() - ToastManager::kProgressBarMargin;
        int y = parentRect.bottom() - height() - ToastManager::kProgressBarMargin - y_offset_;
        move(x, y);
    }
    else
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
        int x = screenGeometry.right() - width() - ToastManager::kProgressBarMargin;
        int y =
            screenGeometry.bottom() - height() - ToastManager::kProgressBarMargin - y_offset_;
        move(x, y);
    }
}
} // namespace gui
} // namespace xequation