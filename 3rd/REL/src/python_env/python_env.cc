// =============================================================================
//  python_env.cc — embedded CPython environment configuration & lifecycle
// =============================================================================

#include "python_env.h"

#include <Python.h>

#include <cstdio>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace xequation {
namespace python {
namespace {

#ifdef _WIN32
// Since Python 3.8, extension modules (.pyd) are loaded with
// LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: PATH is no longer searched for their
// DLL dependencies.  A standalone python.exe still works because its
// application directory is <prefix>/bin (where libopenblas.dll & friends
// live), but an embedding host's directory does not contain them — numpy
// then fails with a misleading "import from source directory" error.
// Register the missing directories via AddDllDirectory before interpreter
// startup so packages like numpy can load their native dependencies.
void register_dll_search_dirs(const PyEnvConfig& cfg)
{
#ifdef REL_PYTHON_DLL_DIR
    // The directory containing the CPython DLL itself (libpython3.X.dll):
    // in prefix-style installs (msys2/MinGW, python.org) the native
    // dependencies of extension modules (libopenblas.dll, ...) live next
    // to it.  Injected by CMake.
    wchar_t* bin_dir = Py_DecodeLocale(REL_PYTHON_DLL_DIR, nullptr);
    if (bin_dir != nullptr)
    {
        AddDllDirectory(bin_dir);
        PyMem_RawFree(bin_dir);
    }
#endif

    // Each configured sys.path entry: wheels may ship DLLs next to the .pyd
    // files (covered by DLL_LOAD_DIR), but sibling sub-directories are not.
    for (const auto& p : cfg.lib_path_list)
    {
        wchar_t* wide = Py_DecodeLocale(p.c_str(), nullptr);
        if (wide != nullptr)
        {
            AddDllDirectory(wide);
            PyMem_RawFree(wide);
        }
    }
}
#endif  // _WIN32

}  // namespace

// ---- static member definitions -------------------------------------------

PyEnvConfig PyEnvManager::config_;
bool PyEnvManager::manage_python_context_ = false;

// ---- configuration --------------------------------------------------------

void PyEnvManager::SetPyEnvConfig(const PyEnvConfig& config)
{
    if (Py_IsInitialized())
        throw std::runtime_error(
            "PyEnvManager::SetPyEnvConfig: cannot change the Python "
            "environment after the interpreter is initialized");
    config_ = config;
}

void PyEnvManager::SetDefaultPyEnvConfig()
{
    // Build-time paths injected by CMake (see CMakeLists.txt, rel_python_env).
    PyEnvConfig config;
#ifdef REL_PYTHON_HOME
    config.py_home = REL_PYTHON_HOME;
#endif
#ifdef REL_PYTHON_STDLIB
    config.lib_path_list.push_back(REL_PYTHON_STDLIB);
#endif
#ifdef REL_PYTHON_LIB_DYNLOAD
    config.lib_path_list.push_back(REL_PYTHON_LIB_DYNLOAD);
#endif
#ifdef REL_PYTHON_SITE_PACKAGES
    config.lib_path_list.push_back(REL_PYTHON_SITE_PACKAGES);
#endif
    SetPyEnvConfig(config);
}

const PyEnvConfig& PyEnvManager::GetPyEnvConfig()
{
    return config_;
}

// ---- lifecycle ------------------------------------------------------------

void PyEnvManager::InitializePyEnv()
{
    // Idempotent: an interpreter that is already alive keeps the current
    // ownership.  When this library created it (a repeated InitializePyEnv
    // call), manage_python_context_ stays true; when the host initialized
    // it elsewhere, manage_python_context_ stays false and this library
    // never finalizes it.
    if (Py_IsInitialized())
        return;

    manage_python_context_ = true;

#ifdef _WIN32
    // Extension-module DLL dependencies (numpy -> libopenblas.dll etc.) must
    // be registered before Py_InitializeFromConfig: since Python 3.8 the
    // loader no longer searches PATH.
    register_dll_search_dirs(config_);
#endif

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    // config.home: Python installation prefix (contains the stdlib).
    // PyConfig_Clear() releases this pointer with PyMem_RawFree, which is
    // exactly what Py_DecodeLocale allocated.
    if (!config_.py_home.empty())
    {
        wchar_t* home = Py_DecodeLocale(config_.py_home.c_str(), nullptr);
        if (home == nullptr)
            throw std::runtime_error(
                "PyEnvManager::InitializePyEnv: failed to decode py_home");
        config.home = home;
    }

    // config.module_search_paths: explicit sys.path.  Setting
    // module_search_paths_set = 1 replaces CPython's default path
    // computation entirely, so the list must be complete (stdlib,
    // lib-dynload, site-packages, ...).
    if (!config_.lib_path_list.empty())
    {
        config.module_search_paths_set = 1;
        for (const auto& path : config_.lib_path_list)
        {
            wchar_t* wide_path = Py_DecodeLocale(path.c_str(), nullptr);
            if (wide_path == nullptr)
            {
                PyConfig_Clear(&config);
                throw std::runtime_error(
                    "PyEnvManager::InitializePyEnv: failed to decode Python "
                    "path: " + path);
            }
            PyStatus append_status =
                PyWideStringList_Append(&config.module_search_paths, wide_path);
            PyMem_RawFree(wide_path);  // PyWideStringList_Append copies
            if (PyStatus_Exception(append_status))
            {
                PyConfig_Clear(&config);
                throw std::runtime_error(
                    "PyEnvManager::InitializePyEnv: failed to append Python "
                    "path: " + path);
            }
        }
    }

    PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    if (PyStatus_Exception(status))
    {
        manage_python_context_ = false;
        if (status.err_msg)
            std::fprintf(stderr, "Python initialization error: %s\n",
                         status.err_msg);
        throw std::runtime_error("Failed to initialize Python environment");
    }

    if (!Py_IsInitialized())
    {
        manage_python_context_ = false;
        throw std::runtime_error("Python initialization failed");
    }
}

void PyEnvManager::ShutdownPyEnv()
{
    // Only finalize interpreters this library created.  A host-owned
    // interpreter keeps running (the host finalizes it, if ever).
    if (!manage_python_context_)
        return;

    if (Py_IsInitialized())
        Py_FinalizeEx();

    manage_python_context_ = false;
}

bool PyEnvManager::IsInitialized()
{
    return Py_IsInitialized() != 0;
}

bool PyEnvManager::ManagePythonContext()
{
    return manage_python_context_;
}

}  // namespace python
}  // namespace xequation
