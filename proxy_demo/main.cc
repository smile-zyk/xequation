#include <QApplication>

#include "proxy_demo_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    xresults::gui::ProxyDemoWidget widget;
    widget.show();

    return app.exec();
}
