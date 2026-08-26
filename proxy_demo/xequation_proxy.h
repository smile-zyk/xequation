#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"

// The concrete Engine / Context types are only used as return references in
// XEquationProxy; forward-declare them here to avoid pulling python/pybind11/
// Python.h and rel implementation headers into the host (GUI) compilation unit.
// forward-declared in the global namespace, for the using directives below.
namespace xequation
{
namespace python
{
class PythonEquationContext;
class PythonEquationEngine;
}
namespace rel_engine
{
class RelEquationContext;
class RelEquationEngine;
}
} // namespace xequation

// Promote API-related xequation types to the global namespace so hosts can
// use them without any prefix.
using ::xequation::Connection;
using ::xequation::Equation;
using ::xequation::EquationAddedCallback;
using ::xequation::EquationException;
using ::xequation::EquationGroup;
using ::xequation::EquationGroupAddedCallback;
using ::xequation::EquationGroupId;
using ::xequation::EquationGroupRemovingCallback;
using ::xequation::EquationGroupUpdateFlag;
using ::xequation::EquationGroupUpdatedCallback;
using ::xequation::EquationManager;
using ::xequation::EquationRemovedCallback;
using ::xequation::EquationRemovingCallback;
using ::xequation::EquationUpdateFlag;
using ::xequation::EquationUpdatedCallback;
using ::xequation::EquationValue;
using ::xequation::InterpretResult;
using ::xequation::ParseException;
using ::xequation::ParseMode;
using ::xequation::ParseResult;
using ::xequation::ResultStatus;
using ::xequation::ScopedConnection;
using ::xequation::DependencyCycleException;
using ::xequation::python::PythonEquationContext;
using ::xequation::python::PythonEquationEngine;
using ::xequation::rel_engine::RelEquationContext;
using ::xequation::rel_engine::RelEquationEngine;

enum class Engine
{
    kPython,
    kRel,
};

// Process-wide singleton facade: holds one Python EquationManager and one REL
// EquationManager for the host to compute/edit/parse expressions.
//
//  - Accessible from anywhere via GetInstance();
//  - Event notifications are passed through as-is from core signals (callback
//    signatures in equation_signals_manager.h); connection management
//    (Connect*/DisconnectAll) is collected in this class;
//  - Protected by a recursive mutex; re-entrant safe inside callbacks;
//  - The Python engine initializes the embedded interpreter lazily on first
//    access (idempotent).
class XEquationProxy
{
  public:
    static XEquationProxy &GetInstance();

    // general Manager access
    EquationManager &manager(Engine engine);

    // ---- concrete Engine / Context access ----
    // Engine is a process-wide singleton; Context is the concrete context held
    // by the corresponding Manager (Python: pybind11::dict wrapper; REL:
    // rel::Environment wrapper); first access lazily creates the engine.
    PythonEquationEngine &python_engine();
    RelEquationEngine &rel_engine();

    PythonEquationContext &python_context();
    RelEquationContext &rel_context();

    // ---- event notifications: pass core signals through as-is ----

    Connection ConnectEquationAdded(Engine engine, EquationAddedCallback callback);
    Connection ConnectEquationRemoving(Engine engine, EquationRemovingCallback callback);
    Connection ConnectEquationRemoved(Engine engine, EquationRemovedCallback callback);
    Connection ConnectEquationUpdated(Engine engine, EquationUpdatedCallback callback);
    Connection ConnectEquationGroupAdded(Engine engine, EquationGroupAddedCallback callback);
    Connection ConnectEquationGroupRemoving(Engine engine, EquationGroupRemovingCallback callback);
    Connection ConnectEquationGroupUpdated(Engine engine, EquationGroupUpdatedCallback callback);
    void DisconnectAll(Engine engine);

    // ---- editing ----

    EquationGroupId AddEquationGroup(Engine engine, const std::string &equation_statement);
    EquationGroupId AddEquation(Engine engine, const std::string &equation_name, const std::string &equation_content);
    void EditEquationGroup(Engine engine, const EquationGroupId &group_id, const std::string &equation_statement);
    void EditSingleEquation(Engine engine, const EquationGroupId &group_id, const std::string &equation_name, const std::string &equation_content);
    void RemoveEquationGroup(Engine engine, const EquationGroupId &group_id);

    // Remove a single Equation by name (in the demo each equation maps to an
    // independent group, so removing it is equivalent to removing that group;
    // triggers kEquationRemoving/kEquationRemoved).
    void RemoveEquation(Engine engine, const std::string &equation_name);

    // ---- parse / compute ----

    ParseResult Parse(Engine engine, const std::string &expression, ParseMode mode);
    InterpretResult Eval(Engine engine, const std::string &expression);
    InterpretResult Exec(Engine engine, const std::string &statement);

    void Update(Engine engine);
    void UpdateEquation(Engine engine, const std::string &equation_name);
    void UpdateEquationGroup(Engine engine, const EquationGroupId &group_id);

    // ---- variable read / query ----

    EquationValue GetValue(Engine engine, const std::string &name);
    std::vector<std::string> GetEquationNames(Engine engine);
    std::vector<std::string> GetContextKeys(Engine engine);
    bool IsEquationExist(Engine engine, const std::string &equation_name);

    // Get Equation by name; can read name/content/status/message/GetValue(),
    // used to replicate rel_demo's ShowEquation display logic in the host.
    const Equation *GetEquation(Engine engine, const std::string &equation_name);
    const EquationGroup *GetEquationGroup(Engine engine, const EquationGroupId &group_id);

  private:
    XEquationProxy();
    ~XEquationProxy();
    XEquationProxy(const XEquationProxy &) = delete;
    XEquationProxy &operator=(const XEquationProxy &) = delete;

    EquationManager *GetManager(Engine engine);

    std::unique_ptr<EquationManager> python_manager_;
    std::unique_ptr<EquationManager> rel_manager_;
    std::recursive_mutex mutex_;
};
