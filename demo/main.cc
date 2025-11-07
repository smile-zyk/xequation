#include <QApplication>
#include "demo_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 创建并显示自定义窗口
    DemoWidget widget;
    widget.show();
    
    // 进入主事件循环
    return app.exec();
}