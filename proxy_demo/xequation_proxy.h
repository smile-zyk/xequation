#pragma once

#include <memory>
#include <mutex>

#include "core/equation_manager.h"

// Process-wide singleton that owns the two engine EquationManagers (one
// Python, one REL) and their lifecycle.  It deliberately does NOT forward any
// EquationManager API: hosts use python_manager() / rel_manager() and call
// the core EquationManager methods directly.
//
//  - Accessible from anywhere via GetInstance();
//  - python_manager() lazily initializes the embedded Python interpreter on
//    first access (idempotent); rel_manager() is ready after construction;
//  - Event notifications are obtained via the returned manager's
//    signals_manager() (core API) -- no pass-through helpers here;
//  - Protected by a recursive mutex; re-entrant safe inside callbacks;
//  - The Python context holds a pybind11::dict; ~XEquationProxy destroys it
//    under the GIL (magic-static singleton, so construction order is safe).
class XEquationProxy
{
  public:
    static XEquationProxy &GetInstance();

    /// The Python-engine EquationManager (lazily initializes the interpreter).
    xequation::EquationManager &python_manager();

    /// The REL-engine EquationManager.
    xequation::EquationManager &rel_manager();

  private:
    XEquationProxy();
    ~XEquationProxy();
    XEquationProxy(const XEquationProxy &) = delete;
    XEquationProxy &operator=(const XEquationProxy &) = delete;

    std::unique_ptr<xequation::EquationManager> python_manager_;
    std::unique_ptr<xequation::EquationManager> rel_manager_;
    std::recursive_mutex mutex_;
};
