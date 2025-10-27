#include "py_expr_engine.h"
#include "core/expr_common.h"
#include "py_restricted_evaluator.h"
#include "py_symbol_extractor.h"
#include "python/py_expr_context.h"
#include <memory>
#include <pybind11/eval.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

using namespace xexprengine;

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
    symbol_extractor_ = std::unique_ptr<PySymbolExtractor>(new PySymbolExtractor());
    restricted_evaluator_ = std::unique_ptr<PyRestrictedEvaluator>(new PyRestrictedEvaluator());

    restricted_evaluator_->RegisterBuiltin("sum");
    restricted_evaluator_->RegisterBuiltin("max");
    restricted_evaluator_->RegisterBuiltin("min");

    restricted_evaluator_->RegisterBuiltin("dict");
    restricted_evaluator_->RegisterBuiltin("list");
}

void PyExprEngine::SetPyEnvConfig(const PyEnvConfig &config)
{
    config_ = config;
}

EvalResult PyExprEngine::Evaluate(const std::string &expr, const ExprContext *context)
{
    const PyExprContext *py_context = dynamic_cast<const PyExprContext *>(context);
    if (py_context == nullptr)
    {
        return EvalResult{std::string("Invalid context"), VariableStatus::kInvalidContext};
    }
    try
    {
        py::object res = restricted_evaluator_->Eval(expr, py_context->dict());
        EvalResult result;
        result.value = res;
        result.status = VariableStatus::kExprEvalSuccess;
        return result;
    }
    catch (const py::error_already_set &e)
    {
        std::string error_msg = e.what();
        VariableStatus error_status = VariableStatus::kInit;

        if (error_msg.find("SyntaxError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalSyntaxError;
        }
        else if (error_msg.find("NameError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalNameError;
        }
        else if (error_msg.find("TypeError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalTypeError;
        }
        else if (error_msg.find("ZeroDivisionError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalZeroDivisionError;
        }
        else if (error_msg.find("ValueError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalValueError;
        }
        else if (error_msg.find("MemoryError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalMemoryError;
        }
        else if (error_msg.find("OverflowError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalOverflowError;
        }
        else if (error_msg.find("RecursionError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalRecursionError;
        }
        else if (error_msg.find("IndexError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalIndexError;
        }
        else if (error_msg.find("KeyError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalKeyError;
        }
        else if (error_msg.find("AttributeError") != std::string::npos)
        {
            error_status = VariableStatus::kExprEvalAttributeError;
        }
        else
        {
            error_status = VariableStatus::kExprEvalValueError;
        }

        return EvalResult{Value::Null(), error_status, e.what()};
    }
}

ParseResult PyExprEngine::Parse(const std::string &expr)
{
    return symbol_extractor_->Extract(expr, restricted_evaluator_->global_symbols());
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
    symbol_extractor_.reset();
    restricted_evaluator_.reset();
    if (manage_python_context_)
    {
        FinalizePyEnv();
    }
}