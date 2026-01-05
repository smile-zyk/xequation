#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include <memory>

#include "ProgressManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("ProgressManager / PopupProgressBar 测试"));
    window.resize(520, 360);

    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setSpacing(12);

    QLabel *title = new QLabel(QStringLiteral("点击下方按钮创建弹出式进度条，演示 ProgressManager 和 PopupProgressBar 的基本用法。"), centralWidget);
    title->setWordWrap(true);
    layout->addWidget(title);

    QMap<QString, QTimer*> activeTimers;

    std::function<void(const QString&)> stopTimer = [&](const QString& id) {
        if (activeTimers.contains(id)) {
            activeTimers[id]->stop();
            activeTimers[id]->deleteLater();
            activeTimers.remove(id);
        }
    };

    std::function<void(const QString&, int, const QString&)> startTask = [&](const QString& id, int intervalMs, const QString& titleText) {
        stopTimer(id);

        PopupProgressBar *bar = ProgressManager::instance()->createProgress(id, titleText, 1800);
        ProgressManager::instance()->setProgressRange(id, 0, 100);
        ProgressManager::instance()->updateProgress(id, 0, QStringLiteral("开始..."));

        QTimer *timer = new QTimer(&window);
        auto progressValue = std::make_shared<int>(0);

        QObject::connect(timer, &QTimer::timeout, &window, [=, &stopTimer]() mutable {
            *progressValue += QRandomGenerator::global()->bounded(8, 18);
            int value = qMin(*progressValue, 100);
            ProgressManager::instance()->updateProgress(id, value, QStringLiteral("处理中 %1%").arg(value));

            if (value >= 100) {
                ProgressManager::instance()->completeProgress(id);
                stopTimer(id);
            }
        });

        QObject::connect(bar, &PopupProgressBar::cancelled, &window, [=, &stopTimer]() {
            stopTimer(id);
        });
        QObject::connect(bar, &PopupProgressBar::completed, &window, [=, &stopTimer]() {
            stopTimer(id);
        });
        QObject::connect(bar, &QObject::destroyed, &window, [=, &stopTimer]() {
            stopTimer(id);
        });

        activeTimers.insert(id, timer);
        timer->start(intervalMs);
    };

    QPushButton *btnShort = new QPushButton(QStringLiteral("开始短任务 (~2s)"), centralWidget);
    QPushButton *btnLong = new QPushButton(QStringLiteral("开始长任务 (~5s)"), centralWidget);
    QPushButton *btnBatch = new QPushButton(QStringLiteral("同时启动三个任务"), centralWidget);
    QPushButton *btnCancelA = new QPushButton(QStringLiteral("取消任务 A"), centralWidget);

    layout->addWidget(btnShort);
    layout->addWidget(btnLong);
    layout->addWidget(btnBatch);
    layout->addWidget(btnCancelA);
    layout->addStretch();

    QObject::connect(btnShort, &QPushButton::clicked, [&]() {
        startTask(QStringLiteral("task_short"), 120, QStringLiteral("任务 A: 短任务"));
    });

    QObject::connect(btnLong, &QPushButton::clicked, [&]() {
        startTask(QStringLiteral("task_long"), 260, QStringLiteral("任务 B: 长任务"));
    });

    QObject::connect(btnBatch, &QPushButton::clicked, [&]() {
        startTask(QStringLiteral("task_batch_1"), 180, QStringLiteral("批量任务 1"));
        startTask(QStringLiteral("task_batch_2"), 210, QStringLiteral("批量任务 2"));
        startTask(QStringLiteral("task_batch_3"), 240, QStringLiteral("批量任务 3"));
    });

    QObject::connect(btnCancelA, &QPushButton::clicked, [&]() {
        ProgressManager::instance()->cancelProgress(QStringLiteral("task_short"));
        stopTimer(QStringLiteral("task_short"));
    });

    window.setCentralWidget(centralWidget);
    window.show();

    return app.exec();
}