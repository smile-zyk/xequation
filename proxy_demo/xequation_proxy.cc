#include "xequation_proxy.h"

#include <Python.h>

#include <pybind11/pybind11.h>

#include "python/python_equation_engine.h"
#include "python_manager.h"
#include "rel_engine/rel_equation_engine.h"

using namespace xequation;
using xequation::python::PythonEquationEngine;
using xequation::rel_engine::RelEquationEngine;

XEquationProxy &XEquationProxy::GetInstance()
{
    // C++11 magic-static: thread-safe construction.  The constructor only creates
    // the REL engine, so it is safe to call from anywhere (incl. static init).
    static XEquationProxy instance;
    return instance;
}

XEquationProxy::XEquationProxy()
{
    rel_manager_ = RelEquationEngine::GetInstance().CreateEquationManager();
}

XEquationProxy::~XEquationProxy()
{
    // The Python context holds a pybind11::dict, whose destruction needs the GIL.
    // The interpreter is still alive before process exit (no Py_FinalizeEx call), so safe here.
    if (python_manager_)
    {
        pybind11::gil_scoped_acquire acquire;
        python_manager_.reset();
    }
    rel_manager_.reset();
}

EquationManager &XEquationProxy::python_manager()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!python_manager_)
    {
        // Initialize the embedded interpreter lazily on first access (idempotent).
        // If uninitialized and the host provides no custom config, inject the
        // build-time defaults (REL_PYTHON_HOME etc.).
        if (!python_manager::PyEnvManager::IsInitialized())
        {
            python_manager::PyEnvManager::SetDefaultPyEnvConfig();
        }
        python_manager_ = PythonEquationEngine::GetInstance().CreateEquationManager();
    }
    return *python_manager_;
}

EquationManager &XEquationProxy::rel_manager()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return *rel_manager_;
}
