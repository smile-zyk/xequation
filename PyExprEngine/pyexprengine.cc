#include "pyexprengine.h"
#include <Python.h>
#include <cpython/initconfig.h>
#include <pylifecycle.h>
#include <stdexcept>

PyExprEngine::PyEnvConfig config_;

PyExprEngine& PyExprEngine::Instance()
{
    static PyExprEngine instance;
    return instance;
}

PyExprEngine::PyExprEngine()
{
    if(Py_IsInitialized()) {
        manage_python_context_ = false;
    } else {
        manage_python_context_ = true;
        InitializePyEnv();
    }
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
