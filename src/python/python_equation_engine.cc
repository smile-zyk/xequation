#include "python_equation_engine.h"
#include "core/equation_common.h"
#include "python_executor.h"
#include "python/python_parser.h"
#include "python/python_equation_context.h"
#include "value_pybind_converter.h"
#include <memory>

using namespace xequation;
using namespace xequation::python;

PythonEquationEngine::PythonEquationEngine()
{
    engine_info_.name = "Python";

    // 环境管理与初始化委托给 REL 的 python_manager（幂等）：
    //   - 解释器已初始化（宿主或先前调用）-> 直接复用，不改变所有权；
    //   - 未初始化 -> 用 python_manager 存储的配置（默认/自定义）初始化。
    // 宿主若需自定义路径，应在 GetInstance() 之前调用
    // python_manager::PyEnvManager::SetPyEnvConfig() / SetDefaultPyEnvConfig()。
    python_manager::PyEnvManager::InitializePyEnv();

    code_parser = std::unique_ptr<PythonParser>(new PythonParser());
    code_executor = std::unique_ptr<PythonExecutor>(new PythonExecutor());

    // 把 PyObject 生命周期操作注入 core（PyObjectRef 延迟 DECREF 依赖它）
    value_convert::InstallPyObjectOps();

    // 让主线程让出 GIL，否则后台线程（TaskManager 线程池等）在
    // gil_scoped_acquire 时会永远等不到 GIL 而死锁。
    // PyEval_SaveThread 保存当前线程状态并释放 GIL。
    g_main_thread_state_ = PyEval_SaveThread();
}

InterpretResult PythonEquationEngine::Eval(const std::string &code, const EquationContext *context)
{
    pybind11::gil_scoped_acquire acquire;
    value_convert::FlushPendingDecrefs();
    const PythonEquationContext* py_context = dynamic_cast<const PythonEquationContext*>(context);
    return code_executor->Eval(code, py_context ? py_context->dict() : pybind11::dict());
}

InterpretResult PythonEquationEngine::Exec(const std::string &code, const EquationContext *context)
{
    pybind11::gil_scoped_acquire acquire;
    value_convert::FlushPendingDecrefs();
    const PythonEquationContext* py_context = dynamic_cast<const PythonEquationContext*>(context);
    return code_executor->Exec(code, py_context ? py_context->dict() : pybind11::dict());
}

ParseResult PythonEquationEngine::Parse(const std::string &code, ParseMode mode)
{
    pybind11::gil_scoped_acquire acquire;
    value_convert::FlushPendingDecrefs();
    if (mode == ParseMode::kExpression)
    {
        return code_parser->ParseExpression(code);
    }
    else {
        return code_parser->ParseStatements(code);
    }
}

std::unique_ptr<EquationContext> PythonEquationEngine::CreateContext()
{
    return std::unique_ptr<EquationContext>(new PythonEquationContext(engine_info_));
}

PythonEquationEngine::~PythonEquationEngine()
{
    // 销毁 pybind11 对象需要持有 GIL。解释器生命周期由 python_manager 管理
    // （宿主可调用 python_manager::PyEnvManager::ShutdownPyEnv() 收尾）。
    if (g_main_thread_state_)
    {
        // 恢复 GIL（对应构造函数里的 PyEval_SaveThread）
        PyEval_RestoreThread(static_cast<PyThreadState *>(g_main_thread_state_));
        g_main_thread_state_ = nullptr;
    }
    pybind11::gil_scoped_acquire acquire;
    code_parser.reset();
    code_executor.reset();
}

void PythonEquationEngine::SetOutputHandler(OutputHandler handler)
{
    if (code_executor)
    {
        code_executor->SetOutputHandler(handler);
    }
}