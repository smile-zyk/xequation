// =============================================================================
//  test_python_env.cc — unit tests for the rel_python_env static library
//  (xequation::python::PyEnvManager).
//
//  Only compiled when BUILD_PYTHON=ON.  Covers:
//    - configuration storage (SetPyEnvConfig / GetPyEnvConfig)
//    - the CMake-injected default configuration (SetDefaultPyEnvConfig)
//    - interpreter initialization + ownership (manage_python_context_)
//    - the host-managed branch (interpreter already initialized elsewhere)
//    - sys.path effectiveness (stdlib / site-packages importable)
// =============================================================================

#include "python_env.h"

#include <Python.h>

#include <string>

#include <gtest/gtest.h>

namespace {

using xequation::python::PyEnvConfig;
using xequation::python::PyEnvManager;

// Every test leaves the interpreter finalized so that config tests (which
// require an uninitialized interpreter) work regardless of execution order.
class PythonEnvTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_FALSE(PyEnvManager::IsInitialized())
            << "previous test left the interpreter running";
    }

    void TearDown() override
    {
        if (PyEnvManager::IsInitialized())
            PyEnvManager::ShutdownPyEnv();
    }
};

// ---------------------------------------------------------------------------
// Configuration storage
// ---------------------------------------------------------------------------

TEST_F(PythonEnvTest, SetAndGetConfig)
{
    PyEnvConfig cfg;
    cfg.py_home = "C:/some/python";
    cfg.lib_path_list.push_back("C:/some/python/lib/python3.12");
    cfg.lib_path_list.push_back("C:/some/python/lib/python3.12/site-packages");

    PyEnvManager::SetPyEnvConfig(cfg);

    const PyEnvConfig& stored = PyEnvManager::GetPyEnvConfig();
    EXPECT_EQ(stored.py_home, "C:/some/python");
    ASSERT_EQ(stored.lib_path_list.size(), 2u);
    EXPECT_EQ(stored.lib_path_list[0], "C:/some/python/lib/python3.12");
    EXPECT_EQ(stored.lib_path_list[1],
              "C:/some/python/lib/python3.12/site-packages");
}

TEST_F(PythonEnvTest, DefaultConfigContainsBuildTimePaths)
{
    PyEnvManager::SetDefaultPyEnvConfig();

    const PyEnvConfig& cfg = PyEnvManager::GetPyEnvConfig();
    EXPECT_FALSE(cfg.py_home.empty());

    // stdlib + lib-dynload + site-packages, injected by CMake.
    ASSERT_EQ(cfg.lib_path_list.size(), 3u);
    EXPECT_NE(cfg.lib_path_list[0].find("python"), std::string::npos);
    EXPECT_NE(cfg.lib_path_list[1].find("lib-dynload"), std::string::npos);
    EXPECT_NE(cfg.lib_path_list[2].find("site-packages"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Initialization + ownership
// ---------------------------------------------------------------------------

TEST_F(PythonEnvTest, InitializeCreatesOwnedInterpreter)
{
    PyEnvManager::SetDefaultPyEnvConfig();
    PyEnvManager::InitializePyEnv();

    EXPECT_TRUE(PyEnvManager::IsInitialized());
    EXPECT_TRUE(PyEnvManager::ManagePythonContext());

    // Idempotent: a second call must not reinitialize or throw.
    PyEnvManager::InitializePyEnv();
    EXPECT_TRUE(PyEnvManager::IsInitialized());

    // Configuration is frozen once the interpreter is alive.
    PyEnvConfig cfg;
    EXPECT_THROW(PyEnvManager::SetPyEnvConfig(cfg), std::runtime_error);

    // Shutdown finalizes because this library owns the interpreter.
    PyEnvManager::ShutdownPyEnv();
    EXPECT_FALSE(PyEnvManager::IsInitialized());
    EXPECT_FALSE(PyEnvManager::ManagePythonContext());
}

TEST_F(PythonEnvTest, HostManagedInterpreterIsNotFinalized)
{
    // Simulate a host that initialized Python itself (fully configured, so
    // the stdlib is found regardless of the working directory).
    PyEnvManager::SetDefaultPyEnvConfig();
    const PyEnvConfig& default_cfg = PyEnvManager::GetPyEnvConfig();

    PyConfig host_config;
    PyConfig_InitPythonConfig(&host_config);
    if (!default_cfg.py_home.empty())
        host_config.home = Py_DecodeLocale(default_cfg.py_home.c_str(), nullptr);
    for (const auto& p : default_cfg.lib_path_list)
    {
        wchar_t* wide = Py_DecodeLocale(p.c_str(), nullptr);
        ASSERT_NE(wide, nullptr);
        PyWideStringList_Append(&host_config.module_search_paths, wide);
        PyMem_RawFree(wide);
    }
    host_config.module_search_paths_set = 1;

    PyStatus status = Py_InitializeFromConfig(&host_config);
    PyConfig_Clear(&host_config);
    ASSERT_FALSE(PyStatus_Exception(status));
    ASSERT_TRUE(Py_IsInitialized());

    PyEnvManager::InitializePyEnv();  // must detect the running interpreter
    EXPECT_TRUE(PyEnvManager::IsInitialized());
    EXPECT_FALSE(PyEnvManager::ManagePythonContext());

    // Shutdown must leave a host-owned interpreter running.
    PyEnvManager::ShutdownPyEnv();
    EXPECT_TRUE(Py_IsInitialized());

    // The host finalizes its own interpreter.
    Py_FinalizeEx();
    EXPECT_FALSE(Py_IsInitialized());
}

// ---------------------------------------------------------------------------
// sys.path effectiveness
// ---------------------------------------------------------------------------

TEST_F(PythonEnvTest, StdlibAndSitePackagesImportable)
{
    PyEnvManager::SetDefaultPyEnvConfig();
    PyEnvManager::InitializePyEnv();

    // Explicit module_search_paths replace CPython's default path
    // computation, so the stdlib must come from the configured list.
    // (Raw C API — this test binary does not link rel_runtime.)
    int rc = PyRun_SimpleString(
        "import sys, json, os\n"
        "assert any('site-packages' in p for p in sys.path)\n"
        "assert os.path.isdir(sys.path[0])\n");
    EXPECT_EQ(rc, 0) << "Python snippet failed (traceback on stderr)";
}

}  // namespace
