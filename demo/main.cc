#include <QApplication>
#include <QtGlobal>

#include "demo_widget.h"

#ifdef DEMO_HAS_PYTHON
#include <exception>
#include "environment.h"     // rel::Environment::CleanupPythonState
#include "python_manager.h"  // python_manager::PyEnvManager
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef DEMO_HAS_PYTHON
    // REL 嵌入式 Python：解释器生命周期归宿主所有（rel 只执行插件，从不
    // 自行创建/销毁解释器，见 python_loader.cc）。与上游 REL main.cc 一致：
    // 先用 CMake 注入的编译期路径（py_home + stdlib / lib-dynload /
    // site-packages）配置默认环境，再初始化。失败不致命：只是 python_plugins
    // 不可用，dataset 加载不受影响。
    try
    {
        python_manager::PyEnvManager::SetDefaultPyEnvConfig();
        python_manager::PyEnvManager::InitializePyEnv();
    }
    catch (const std::exception &e)
    {
        qWarning("Failed to initialize embedded Python: %s", e.what());
    }
#endif

    xresults::gui::DemoWidget widget;
    widget.show();

    const int result = app.exec();

#ifdef DEMO_HAS_PYTHON
    // 逆序清理：先释放 pybind11 持有的回调（解释器必须还活着），再销毁解释器。
    if (python_manager::PyEnvManager::IsInitialized())
    {
        rel::Environment::CleanupPythonState();
        python_manager::PyEnvManager::ShutdownPyEnv();
    }
#endif

    return result;
}
