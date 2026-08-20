#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "value.h"  // rel::Value
#include "dataset.h"
#include "function.h"

#include <rapidjson/document.h>

namespace rel {

// =========================================================================
//  EnvironmentConfig -- persistent dataset + plugin context (JSON)
// =========================================================================
//
//  JSON schema:
//  {
//    "datasets": [
//      { "name": "noise",    "format": "hdf5", "path": "/data/noise.xdataset" },
//      { "name": "amplifier","format": "hdf5", "path": "/data/amp.xdataset" }
//    ],
//    "default_dataset": "noise",
//    "python_plugins": ["plugins/snr.py", "plugins/eye.py"]
//  }
//
//  "default_dataset" is optional; if omitted, the first dataset in "datasets"
//  becomes the default.
//  "python_plugins" is optional; each entry is a .py plugin path resolved
//  relative to the config file's directory (requires BUILD_PYTHON=ON).

struct REL_API DatasetConfig {
    std::string name;
    std::string format;
    std::string path;
};

struct REL_API EnvironmentConfig {
    std::vector<DatasetConfig> datasets;
    std::string               default_dataset;
    std::vector<std::string>  python_plugins;

    /// Load from a JSON file.
    static EnvironmentConfig Load(const std::string& config_path);

private:
    static DatasetConfig     ParseDataset(const rapidjson::Value& v);
    static EnvironmentConfig Parse(const rapidjson::Document& doc);
};

// =========================================================================
//  Environment -- flat variable table + global shared context
// =========================================================================
//
//  A pure storage container for user-defined variables only.
//
//  Builtin constants, functions, and datasets are stored in global (static)
//  registries shared across all Environment instances.  This means:
//    - InitBuiltinConstants() / InitBuiltinFunctions() are called once.
//    - LoadFromConfig() loads datasets/plugins globally, not per-env.
//    - Define() rejects names that collide with builtin constants.
//

class REL_API Environment
{
public:
    Environment() = default;

    // ---- variables (instance -- user-defined only) ----

    /// Bind or rebind a name.  Overwrites existing bindings silently.
    /// Throws std::runtime_error when `name` collides with a builtin constant.
    void Define(const std::string& name, rel::Value value);

    /// Look up a user-defined variable in the flat table.
    /// Returns default Value when not found.
    rel::Value Get(const std::string& name) const;

    /// Remove a user-defined variable by name.
    /// Returns true when the variable existed and was removed.
    bool Remove(const std::string& name);

    /// Remove all user-defined variables (constants / functions / datasets
    /// registered globally are unaffected).
    void Clear();

    /// Names of all user-defined variables (unordered).
    std::vector<std::string> VariableNames() const;

    // ---- static: builtin constants ----------------------------------------

    /// Populate the global builtin-constant registry (PI, e, c0, ...).
    /// Call once during process startup.
    static void InitBuiltinConstants();

    /// Look up a builtin constant by name, or nullptr when not found.
    static const rel::Value* FindConstant(const std::string& name);

    /// Names of all registered builtin constants (unordered).
    static std::vector<std::string> ConstantNames();

    // ---- static: function registry ----------------------------------------

    /// Register REL's builtin function libraries ("builtin" + "math").
    /// Call once during process startup.
    static void InitBuiltinFunctions();

    /// Register a function in the global registry.
    /// Overwrites an existing registration with the same name silently.
    static void RegisterFunction(Function fn);

    /// Register every function in a library.
    static void RegisterLibrary(const FunctionLibrary& lib);

    /// Remove a registered function by name.
    /// Returns true when the function existed and was removed.
    static bool UnregisterFunction(const std::string& name);

    /// Check whether a function of that name is registered.
    /// Thread-safe existence check; use CopyFunction() to obtain a callable
    /// copy (a raw pointer/reference into the registry would escape the
    /// registry lock and could dangle on a concurrent re-registration).
    static bool HasFunction(const std::string& name);

    /// Thread-safe copy of a registered function, or false when not found.
    /// Copies the Function while holding the registry lock, so the returned
    /// copy can be invoked without racing a concurrent re-registration.
    static bool CopyFunction(const std::string& name, Function& out);

    /// Names of all registered functions (unordered).
    static std::vector<std::string> FunctionNames();

    /// Look up a registered function by name and invoke it with the given
    /// resolved arguments.  Throws std::runtime_error when the function is
    /// not registered.  The lookup copies the Function before invoking, so an
    /// implementation that registers more functions (and rehashes the
    /// registry) cannot invalidate the held pointer.
    static rel::Value CallFunction(const std::string& name,
                                   const Function::ArgMap& args);

    /// Positional form: arguments bind to the function's declared parameters
    /// in declaration order (the i-th argument maps to params()[i].name).
    /// Throws std::runtime_error when more positional args are given than the
    /// function declares.
    static rel::Value CallFunction(const std::string& name,
                                   const std::vector<rel::Value>& args);

    /// Variadic positional convenience: CallFunction("sin", x) or
    /// CallFunction("max2", a, b).  Forwards to the vector overload.
    template <typename... Args>
    static rel::Value CallFunction(const std::string& name, Args&&... args)
    {
        // Build the positional argument vector via a braced-init-list.  It is
        // bound to a const reference (lifetime-extended) so overload
        // resolution picks the non-template vector overload instead of this
        // template -- the universal-reference parameter would otherwise win
        // and recurse infinitely.
        const std::vector<rel::Value>& cargs =
            std::vector<rel::Value>{ std::forward<Args>(args)... };
        return CallFunction(name, cargs);
    }

    // ---- static: dataset registry -----------------------------------------

    /// Register a Dataset, transferring ownership to the global registry.
    static void AddDataset(std::unique_ptr<xdataset::Dataset> ds);

    /// Remove a Dataset by name.  If it is the current default,
    /// the default is cleared.  Returns the removed Dataset, or nullptr.
    static std::unique_ptr<xdataset::Dataset> RemoveDataset(const std::string& name);

    /// Set the default Dataset for unqualified references.
    static void SetDefaultDataset(const std::string& name);

    /// The current default Dataset, or nullptr.
    static xdataset::Dataset* DefaultDataset();

    /// Names of all registered datasets (unordered).
    static std::vector<std::string> DatasetNames();

    // ---- static: persistent context ---------------------------------------

    /// Load datasets and plugins from a JSON config file.
    ///   - "datasets":         array of {name, format, path}
    ///   - "default_dataset":  optional, defaults to first dataset
    ///   - "plugin":           optional array of plugin shared-library paths
    /// Existing global registries (datasets, functions) are preserved.
    /// Call once during process startup.
    static void LoadFromConfig(const std::string& config_path);

    // ---- direct lookups (AST-free) ----------------------------------------

    /// Look up a name in user variables, then builtin constants.
    /// Returns nullptr when not found in either.
    const rel::Value* LookupVariableOrConstant(const std::string& name) const;

    /// Find a registered Dataset by name, or nullptr if not found.
    static xdataset::Dataset* FindDataset(const std::string& name);

    // ---- Python plugin (BUILD_PYTHON=ON) ----------------------------------

    /// Execute a Python plugin file.  The interpreter is initialized lazily
    /// on first use.  `register_function()` calls in the script register
    /// functions into the global registry.  Returns true on success, false
    /// on a Python error.  Throws std::runtime_error when built without
    /// Python support (BUILD_PYTHON=OFF).
    static bool LoadPython(const std::string& path);

    /// Execute a snippet of Python code (same semantics as LoadPython).
    static bool ExecPython(const std::string& code);

    /// True when the embedded Python interpreter is available (compiled in).
    /// Returns false when built with BUILD_PYTHON=OFF.
    static bool IsPythonAvailable();

    /// Release all Python plugin state while the interpreter is still alive:
    /// unregister Python-registered functions and drop every pybind11::function
    /// held by the callback registry (under the GIL).  Does NOT finalize the
    /// interpreter -- that is owned by python_manager
    /// (python_manager::PyEnvManager::ShutdownPyEnv), which the host must
    /// call AFTER this cleanup.  No-op when built without Python support.
    static void CleanupPythonState();

private:
    std::unordered_map<std::string, rel::Value> variables_;

    // ---- global (static) state --------------------------------------------
    static std::unordered_map<std::string, rel::Value>
        builtin_constants_;
    static std::unordered_map<std::string, Function>
        functions_;
    static std::mutex functions_mutex_;  // guards functions_
    static std::unordered_map<std::string, std::unique_ptr<xdataset::Dataset>>
        datasets_;
    static std::string default_dataset_name_;
};

} // namespace rel
