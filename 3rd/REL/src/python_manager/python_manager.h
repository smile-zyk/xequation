#pragma once

// =============================================================================
//  python_manager.h -- embedded CPython environment configuration & lifecycle
// =============================================================================
//
//  A small static library (python_manager, formerly rel_python_env) that
//  owns the *environment* side of the embedded interpreter: where Python
//  lives (home) and where modules are found (sys.path), plus
//  initialization / finalization.
//
//  It deliberately uses only the CPython C API (no pybind11), so a host can
//  manage the interpreter without pulling in the binding layer.  The REL
//  runtime (rel) never creates or destroys an interpreter itself;
//  the host (rel_cli.exe / rel_test) does:
//
//      python_manager::PyEnvConfig cfg;
//      cfg.py_home       = ...;                 // Python installation prefix
//      cfg.lib_path_list = { stdlib, lib-dynload, site-packages };
//      python_manager::PyEnvManager::SetPyEnvConfig(cfg);
//      python_manager::PyEnvManager::InitializePyEnv();
//      ...
//      rel::Environment::CleanupPythonState();  // drop callbacks first
//      python_manager::PyEnvManager::ShutdownPyEnv();
// =============================================================================

#include <string>
#include <vector>

namespace python_manager {

/// Configuration of the embedded CPython environment.
struct PyEnvConfig
{
    /// Python "home": the installation prefix that contains the standard
    /// library (e.g. C:/msys64/mingw64).  Empty = let CPython compute it.
    std::string py_home;

    /// Explicit module search paths (sys.path).  When non-empty, CPython's
    /// default path computation is REPLACED entirely by this list, so it
    /// must contain everything plugins need:
    ///   - the stdlib directory        (<prefix>/lib/pythonX.Y)
    ///   - the lib-dynload directory   (<prefix>/lib/pythonX.Y/lib-dynload)
    ///   - the site-packages directory (<prefix>/lib/pythonX.Y/site-packages)
    /// Empty = default path computation (PYTHONPATH, site, ...).
    std::vector<std::string> lib_path_list;
};

/// Manages the embedded CPython interpreter lifecycle.
///
/// All members are static; there is exactly one interpreter per process.
class PyEnvManager
{
public:
    /// Set the configuration used by InitializePyEnv().  Must be called
    /// BEFORE InitializePyEnv(); throws std::runtime_error when the
    /// interpreter is already initialized.
    static void SetPyEnvConfig(const PyEnvConfig& config);

    /// Set the default configuration computed by CMake at build time:
    ///   py_home        = the Python installation prefix (sys.base_prefix)
    ///   lib_path_list  = { stdlib, lib-dynload, site-packages }
    /// so plugins can import the stdlib, C extension modules and pip
    /// packages regardless of the process working directory or PYTHONPATH.
    /// Must be called BEFORE InitializePyEnv().
    static void SetDefaultPyEnvConfig();

    /// The currently stored configuration.
    static const PyEnvConfig& GetPyEnvConfig();

    /// Initialize the embedded interpreter.  Idempotent:
    ///   - already initialized -> ownership is left unchanged.  If this
    ///     library created it (a repeated call), it stays owned here; if the
    ///     host initialized it elsewhere, it stays host-owned;
    ///   - not initialized -> owned by this library, initialized from the
    ///     stored PyEnvConfig via Py_InitializeFromConfig.
    /// Throws std::runtime_error on failure.
    static void InitializePyEnv();

    /// Finalize the interpreter, but ONLY when this library created it
    /// (manage_python_context_ == true).  A host-owned interpreter is left
    /// running.  Call rel::Environment::CleanupPythonState() first so no
    /// live Python object references outlive finalization.
    ///
    /// Precondition (CPython): Py_FinalizeEx must be called from the thread
    /// that initialized Python, and NO other thread may hold the GIL or be
    /// executing Python code.  Stop any worker threads that call into Python
    /// before invoking this.
    static void ShutdownPyEnv();

    /// True when a Python interpreter is currently initialized.
    static bool IsInitialized();

    /// True when this library created (and therefore will finalize) the
    /// interpreter; false when the host owns it.
    static bool ManagePythonContext();

private:
    static PyEnvConfig config_;
    static bool manage_python_context_;
};

}  // namespace python_manager
