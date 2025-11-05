#include "py_expr_engine.h"
#include "core/expr_common.h"
#include "python/py_expr_context.h"
#include <memory>
#include <pybind11/eval.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

using namespace xequation;
using namespace xequation::python;

PyExprEngine::PyEnvConfig PyExprEngine::config_;

PyExprEngine::PyExprEngine()
{
    if (Py_IsInitialized())
    {
        manage_python_context_ = false;
    }
    else
    {
        manage_python_context_ = true;
        InitializePyEnv();
    }
}

void PyExprEngine::SetPyEnvConfig(const PyEnvConfig &config)
{
    config_ = config;
}

ExecResult PyExprEngine::Exec(const std::string &code, const ExprContext *context)
{
    const PyExprContext* py_context = dynamic_cast<const PyExprContext*>(context);
    return code_executor->Exec(code, py_context->dict());
}

ParseResult PyExprEngine::Parse(const std::string &code)
{
    return code_parser->ParseMultipleStatements(code);
}

EvalResult PyExprEngine::Eval(const std::string &code, const ExprContext *context)
{
    const PyExprContext* py_context = dynamic_cast<const PyExprContext*>(context);
    return code_executor->Eval(code, py_context->dict());
}

std::unique_ptr<ExprContext> PyExprEngine::CreateContext()
{
    return std::unique_ptr<ExprContext>(new PyExprContext());
}

void PyExprEngine::InitializePyEnv()
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
}

void PyExprEngine::FinalizePyEnv()
{
    Py_Finalize();
}

PyExprEngine::~PyExprEngine()
{
    code_parser.reset();
    code_executor.reset();
    if (manage_python_context_)
    {
        FinalizePyEnv();
    }
}