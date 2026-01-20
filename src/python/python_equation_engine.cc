#include "python_equation_engine.h"
#include "core/equation_common.h"
#include "python_executor.h"
#include "python/python_parser.h"
#include "python/python_equation_context.h"
#include "value_pybind_converter.h"
#include <memory>

using namespace xequation;
using namespace xequation::python;

PythonEquationEngine::PyEnvConfig PythonEquationEngine::config_;
static PyThreadState* g_main_thread_state = nullptr;

PythonEquationEngine::PythonEquationEngine()
{
    engine_info_.name = "Python";
    
    if (Py_IsInitialized())
    {
        manage_python_context_ = false;
    }
    else
    {
        manage_python_context_ = true;
        InitializePyEnv();
    }
    code_parser = std::unique_ptr<PythonParser>(new PythonParser());
    code_executor = std::unique_ptr<PythonExecutor>(new PythonExecutor());
    value_convert::RegisterPybindValueCallbacksOnce();
}

void PythonEquationEngine::SetPyEnvConfig(const PyEnvConfig &config)
{
    config_ = config;
}

InterpretResult PythonEquationEngine::Interpret(const std::string &code, const EquationContext *context, InterpretMode mode)
{
    pybind11::gil_scoped_acquire acquire;
    const PythonEquationContext* py_context = dynamic_cast<const PythonEquationContext*>(context);
    if (mode == InterpretMode::kEval)
    {
        return code_executor->Eval(code, py_context ? py_context->dict() : pybind11::dict());
    }
    else
    {
        return code_executor->Exec(code, py_context ? py_context->dict() : pybind11::dict());
    }
}

ParseResult PythonEquationEngine::Parse(const std::string &code, ParseMode mode)
{
    pybind11::gil_scoped_acquire acquire;
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

void PythonEquationEngine::InitializePyEnv()
{
    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    if (!config_.py_home.empty())
    {
        config.home = Py_DecodeLocale(config_.py_home.c_str(), nullptr);
    }

    if (!config_.lib_path_list.empty())
    {
        config.module_search_paths_set = 1;
        for (const auto &path : config_.lib_path_list)
        {
            wchar_t *wide_path = Py_DecodeLocale(path.c_str(), nullptr);
            if (wide_path == nullptr)
            {
                PyErr_SetString(PyExc_RuntimeError, "Failed to decode Python path");
                throw std::runtime_error("Failed to decode Python path");
            }
            PyWideStringList_Append(&config.module_search_paths, wide_path);
            PyMem_RawFree(wide_path);
        }
    }

    PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status))
    {
        if (status.err_msg)
        {
            fprintf(stderr, "Python initialization error: %s\n", status.err_msg);
        }
        throw std::runtime_error("Failed to initialize Python environment");
    }

    if (!Py_IsInitialized())
    {
        throw std::runtime_error("Python initialization failed");
    }

    // Release the GIL from the main thread so background threads can acquire it.
    g_main_thread_state = PyEval_SaveThread();
}

PythonEquationEngine::~PythonEquationEngine()
{
    if (manage_python_context_)
    {
        // Restore GIL before destroying Python objects
        if (g_main_thread_state)
        {
            PyEval_RestoreThread(g_main_thread_state);
            g_main_thread_state = nullptr;
        }
        
        // Destroy pybind11 objects while holding GIL
        code_parser.reset();
        code_executor.reset();
        
        // Now finalize Python
        Py_Finalize();
    }
    else
    {
        // If we don't manage the context, destroy objects with GIL acquired
        pybind11::gil_scoped_acquire acquire;
        code_parser.reset();
        code_executor.reset();
    }
}