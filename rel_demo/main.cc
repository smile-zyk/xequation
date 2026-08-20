#include <QApplication>

#include "rel_demo_widget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    xequation::rel_demo::RelDemoWidget widget;
    widget.show();

    return app.exec();
}
