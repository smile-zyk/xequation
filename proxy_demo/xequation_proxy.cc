#include "xequation_proxy.h"

#include <stdexcept>

#include <Python.h>

#include <pybind11/pybind11.h>

#include "python/python_equation_context.h"
#include "python/python_equation_engine.h"
#include "python_manager.h"
#include "rel_engine/rel_equation_context.h"
#include "rel_engine/rel_equation_engine.h"

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

EquationManager *XEquationProxy::GetManager(Engine engine)
{
    switch (engine)
    {
    case Engine::kPython:
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
        return python_manager_.get();
    case Engine::kRel:
        return rel_manager_.get();
    }
    throw std::invalid_argument("XEquationProxy: unknown engine");
}

EquationManager &XEquationProxy::manager(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return *GetManager(engine);
}

// ---- concrete Engine / Context access ----------------------------------

PythonEquationEngine &XEquationProxy::python_engine()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(Engine::kPython); // trigger lazy creation
    return PythonEquationEngine::GetInstance();
}

RelEquationEngine &XEquationProxy::rel_engine()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(Engine::kRel); // trigger lazy creation
    return RelEquationEngine::GetInstance();
}

PythonEquationContext &XEquationProxy::python_context()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    xequation::EquationContext &ctx = GetManager(Engine::kPython)->context();
    return dynamic_cast<PythonEquationContext &>(ctx);
}

RelEquationContext &XEquationProxy::rel_context()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    xequation::EquationContext &ctx = GetManager(Engine::kRel)->context();
    return dynamic_cast<RelEquationContext &>(ctx);
}

// ---- event notifications: pass core signals through as-is ----------------

Connection XEquationProxy::ConnectEquationAdded(Engine engine, EquationAddedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationAdded>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationRemoving(Engine engine, EquationRemovingCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationRemoving>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationRemoved(Engine engine, EquationRemovedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationRemoved>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationUpdated(Engine engine, EquationUpdatedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationUpdated>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupAdded(Engine engine, EquationGroupAddedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupAdded>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupRemoving(Engine engine, EquationGroupRemovingCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupRemoving>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupUpdated(Engine engine, EquationGroupUpdatedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupUpdated>(std::move(callback));
}

void XEquationProxy::DisconnectAll(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->signals_manager().DisconnectAllEvent();
}

// ---- editing ------------------------------------------------------------------

EquationGroupId XEquationProxy::AddEquationGroup(Engine engine, const std::string &equation_statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->AddEquationGroup(equation_statement);
}

EquationGroupId XEquationProxy::AddEquation(Engine engine, const std::string &equation_name, const std::string &equation_content)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->AddEquation(equation_name, equation_content);
}

void XEquationProxy::EditEquationGroup(Engine engine, const EquationGroupId &group_id, const std::string &equation_statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->EditEquationGroup(group_id, equation_statement);
}

void XEquationProxy::EditSingleEquation(Engine engine, const EquationGroupId &group_id, const std::string &equation_name, const std::string &equation_content)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->EditSingleEquation(group_id, equation_name, equation_content);
}

void XEquationProxy::RemoveEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->RemoveEquationGroup(group_id);
}

void XEquationProxy::RemoveEquation(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Locate the group that owns the equation (each equation has its own group in
    // the demo); remove the whole group.  If the equation does not exist,
    // RemoveEquationGroup throws EquationNotFound.
    const Equation *equation = GetManager(engine)->GetEquation(equation_name);
    if (!equation)
    {
        throw xequation::EquationException::EquationNotFound(equation_name);
    }
    GetManager(engine)->RemoveEquationGroup(equation->group_id());
}

// ---- parse / compute ----------------------------------------------------------

ParseResult XEquationProxy::Parse(Engine engine, const std::string &expression, ParseMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Parse(expression, mode);
}

InterpretResult XEquationProxy::Eval(Engine engine, const std::string &expression)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Eval(expression);
}

InterpretResult XEquationProxy::Exec(Engine engine, const std::string &statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Exec(statement);
}

void XEquationProxy::Update(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->Update();
}

void XEquationProxy::UpdateEquation(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->UpdateEquation(equation_name);
}

void XEquationProxy::UpdateEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->UpdateEquationGroup(group_id);
}

// ---- variable read / query -------------------------------------------------------

EquationValue XEquationProxy::GetValue(Engine engine, const std::string &name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->context().Get(name);
}

std::vector<std::string> XEquationProxy::GetEquationNames(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquationNames();
}

std::vector<std::string> XEquationProxy::GetContextKeys(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto keys = GetManager(engine)->context().keys();
    return std::vector<std::string>(keys.begin(), keys.end());
}

bool XEquationProxy::IsEquationExist(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->IsEquationExist(equation_name);
}

const Equation *XEquationProxy::GetEquation(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquation(equation_name);
}

const EquationGroup *XEquationProxy::GetEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquationGroup(group_id);
}
