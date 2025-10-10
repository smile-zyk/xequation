#include "py_expr_engine.h"
#include "python/py_expr_context.h"

using namespace xexprengine;

PyExprEngine::PyEnvConfig PyExprEngine::config_;

PyExprEngine::PyExprEngine()
{
    if(Py_IsInitialized()) 
    {
        manage_python_context_ = false;
    }
    else 
    {
        manage_python_context_ = true;
        InitializePyEnv();
    }
}

void PyExprEngine::SetPyEnvConfig(const PyEnvConfig& config)
{
    config_ = config;
}

EvalResult PyExprEngine::Evaluate(const std::string &expr, const ExprContext *context)
{
    const PyExprContext* py_context = dynamic_cast<const PyExprContext*>(context);
    if(py_context == nullptr) {
        return EvalResult{std::string("Invalid context"), VariableStatus::kInvalidContext};
    }
    py::gil_scoped_acquire acquire;
    try {
        py::object res = py::eval(expr, py::globals(), py_context->dict());
        EvalResult result;
        result.value = res;
        result.status = VariableStatus::kExprEvalSuccess;
        return result;
    } 
    catch (const py::error_already_set &e) 
    {
        return EvalResult{Value::Null(), VariableStatus::kExprEvalSyntaxError, e.what()};
    }
}

ParseResult PyExprEngine::Parse(const std::string &expr)
{
    return symbol_extractor_.Extract(expr);
}

std::unique_ptr<ExprContext> PyExprEngine::CreateContext()
{
    return std::unique_ptr<ExprContext>(new PyExprContext());
}

void PyExprEngine::InitializePyEnv()
{
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    
    if (!config_.py_home.empty()) {
        config.home = Py_DecodeLocale(config_.py_home.c_str(), nullptr);
    }
    
    if (!config_.lib_path_list.empty()) {
        config.module_search_paths_set = 1;
        for (const auto& path : config_.lib_path_list) {
            wchar_t* wide_path = Py_DecodeLocale(path.c_str(), nullptr);
            if (wide_path == nullptr) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to decode Python path");
                throw std::runtime_error("Failed to decode Python path");
            }
            PyWideStringList_Append(&config.module_search_paths, wide_path);
            PyMem_RawFree(wide_path);
        }
    }

    PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        if (status.err_msg) {
            fprintf(stderr, "Python initialization error: %s\n", status.err_msg);
        }
        throw std::runtime_error("Failed to initialize Python environment");
    }
    
    if (!Py_IsInitialized()) {
        throw std::runtime_error("Python initialization failed");
    }
}

void PyExprEngine::FinalizePyEnv()
{
     Py_Finalize();
}

PyExprEngine::~PyExprEngine()
{
    if(manage_python_context_) {
        FinalizePyEnv();
    }
}