// PopupProgressBar.cpp
#include "PopupProgressBar.h"
#include <QStyleOption>
#include <QPainter>
#include <QStyle>

QList<int> PopupProgressBar::s_occupiedOffsets = QList<int>();

PopupProgressBar::PopupProgressBar(const QString& title, int duration, QWidget* parent)
    : QWidget(parent)
    , m_duration(duration)
    , m_yOffset(-1)
    , m_isCompleted(false)
    , m_isCancelled(false)
{
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    
    // 设置样式
    setStyleSheet(R"(
        PopupProgressBar {
            background-color: white;
            border: 1px solid #cccccc;
            border-radius: 8px;
        }
        QProgressBar {
            border: 1px solid #cccccc;
            border-radius: 3px;
            text-align: center;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 3px;
        }
        QLabel {
            color: #333333;
        }
        QPushButton {
            padding: 3px 8px;
            border: 1px solid #cccccc;
            border-radius: 3px;
            background-color: #f5f5f5;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
    )");
    
    // 创建UI
    setupUI();
    setWindowTitle(title);
    m_titleLabel->setText(title);
    
    // 设置大小
    setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // 获取Y轴偏移
    m_yOffset = getNextYOffset();

    // 监听父窗口移动/尺寸变化，保持相对右下角
    if (parentWidget()) {
        parentWidget()->installEventFilter(this);
    }
    
    // 创建动画效果
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);
    
    // 创建定时器
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setSingleShot(true);
    connect(m_fadeTimer, &QTimer::timeout, this, &PopupProgressBar::startFadeOut);
    
    m_closeTimer = new QTimer(this);
    m_closeTimer->setSingleShot(true);
    
}

PopupProgressBar::~PopupProgressBar()
{
    releaseYOffset(m_yOffset);
}

void PopupProgressBar::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    
    // 标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    
    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    
    m_closeButton = new QPushButton("×", this);
    m_closeButton->setFixedSize(20, 20);
    m_closeButton->setStyleSheet("font-size: 16px;");
    connect(m_closeButton, &QPushButton::clicked, this, &PopupProgressBar::onCloseButtonClicked);
    
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    
    // 消息标签
    m_messageLabel = new QLabel("正在处理...", this);
    m_messageLabel->setStyleSheet("color: #666666; font-size: 11px;");
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton("取消", this);
    connect(m_cancelButton, &QPushButton::clicked, this, &PopupProgressBar::onCancelClicked);
    
    buttonLayout->addWidget(m_cancelButton);
    
    // 添加到主布局
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_messageLabel);
    mainLayout->addLayout(buttonLayout);
}

void PopupProgressBar::moveToBottomRight()
{
    if (parentWidget()) {
        QRect parentRect = parentWidget()->geometry();
        int x = parentRect.right() - WINDOW_WIDTH - MARGIN;
        int y = parentRect.bottom() - WINDOW_HEIGHT - MARGIN - m_yOffset;
        move(x, y);
    } else {
        QScreen* screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
        int x = screenGeometry.right() - WINDOW_WIDTH - MARGIN;
        int y = screenGeometry.bottom() - WINDOW_HEIGHT - MARGIN - m_yOffset;
        move(x, y);
    }
}

void PopupProgressBar::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    moveToBottomRight();
}

bool PopupProgressBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
            moveToBottomRight();
        }
    }
    return QWidget::eventFilter(watched, event);
}

int PopupProgressBar::getNextYOffset()
{
    int offset = 0;
    while (s_occupiedOffsets.contains(offset)) {
        offset += WINDOW_HEIGHT + 10;  // 10像素的间距
    }
    s_occupiedOffsets.append(offset);
    return offset;
}

void PopupProgressBar::releaseYOffset(int offset)
{
    s_occupiedOffsets.removeAll(offset);
}

void PopupProgressBar::setProgress(int value)
{
    m_progressBar->setValue(value);
    
    // 如果进度完成，启动自动关闭定时器
    if (value >= m_progressBar->maximum() && !m_isCompleted && !m_isCancelled) {
        complete();
    }
}

void PopupProgressBar::setRange(int min, int max)
{
    m_progressBar->setRange(min, max);
}

void PopupProgressBar::setMessage(const QString& message)
{
    m_messageLabel->setText(message);
}

void PopupProgressBar::complete()
{
    if (m_isCompleted || m_isCancelled) return;
    
    m_isCompleted = true;
    m_progressBar->setValue(m_progressBar->maximum());
    m_messageLabel->setText("已完成");
    m_cancelButton->setEnabled(false);
    
    // 启动淡出定时器
    m_fadeTimer->start(m_duration);
    emit completed();
}

void PopupProgressBar::cancel()
{
    onCancelClicked();
}

void PopupProgressBar::onCancelClicked()
{
    if (m_isCancelled || m_isCompleted) return;
    
    m_isCancelled = true;
    m_progressBar->setValue(0);
    m_messageLabel->setText("已取消");
    m_cancelButton->setEnabled(false);
    
    // 立即开始淡出
    startFadeOut();
    emit cancelled();
}

void PopupProgressBar::onCloseButtonClicked()
{
    // 关闭按钮立即关闭窗口
    close();
}

void PopupProgressBar::startFadeOut()
{
    if (!m_fadeTimer->isActive()) {
        m_fadeTimer->stop();
    }
    
    // 创建淡出动画
    QPropertyAnimation* fadeAnimation = new QPropertyAnimation(this, "opacity");
    fadeAnimation->setDuration(1000);  // 1秒淡出
    fadeAnimation->setStartValue(m_opacityEffect->opacity());
    fadeAnimation->setEndValue(0.0);
    fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
    
    connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        close();
    });
    
    fadeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

float PopupProgressBar::opacity() const
{
    return m_opacityEffect ? m_opacityEffect->opacity() : 1.0;
}

void PopupProgressBar::setOpacity(float opacity)
{
    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(opacity);
    }
}

void PopupProgressBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    painter.setBrush(QBrush(QColor(255, 255, 255, 240)));
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
    
    // 绘制阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 30));
    painter.drawRoundedRect(rect().translated(2, 2), 8, 8);
}