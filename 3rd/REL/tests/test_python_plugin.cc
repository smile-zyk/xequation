// =============================================================================
//  test_python_plugin.cc — unit tests for the embedded Python plugin bridge.
//
//  Only compiled when BUILD_PYTHON=ON (see CMakeLists.txt).  These tests drive
//  the bridge through the public Environment API (ExecPython / LoadPython /
//  CallFunction), so they do not depend on pybind11 headers directly.
//
//  Python-side behaviour is asserted with `assert` inside the snippets; a
//  failing assert raises an AssertionError that ExecPython surfaces as a
//  std::runtime_error (with the traceback in the message), failing the test.
// =============================================================================

#include "environment.h"
#include "value.h"

#ifdef REL_HAS_PYTHON
#include "python_manager.h"
#endif

#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#define REL_MKDIR(p) _mkdir(p)
#define REL_RMDIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define REL_MKDIR(p) mkdir(p, 0755)
#define REL_RMDIR(p) rmdir(p)
#endif

#include <gtest/gtest.h>

namespace {

// ExecPython returns false or throws on a Python error; this macro runs a
// snippet and reports the traceback on failure.
#define RUN_PY(code)                                                     \
    do                                                                   \
    {                                                                    \
        try                                                              \
        {                                                                \
            ASSERT_TRUE(rel::Environment::ExecPython(code));             \
        }                                                                \
        catch (const std::exception& e)                                  \
        {                                                                \
            FAIL() << "Python error: " << e.what();                      \
        }                                                                \
    } while (0)

// Whether the embedded interpreter can `import numpy` (interop tests skip
// when numpy is not installed).
bool g_numpy_available = false;

class PythonPluginTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // The runtime no longer creates an interpreter lazily; the test
        // binary owns the embedded interpreter lifecycle (python_manager).
        python_manager::PyEnvManager::SetDefaultPyEnvConfig();
        python_manager::PyEnvManager::InitializePyEnv();

        rel::Environment::InitBuiltinConstants();
        rel::Environment::InitBuiltinFunctions();
        try
        {
            g_numpy_available = rel::Environment::ExecPython("import numpy");
        }
        catch (...)
        {
            g_numpy_available = false;
        }
    }

    static void TearDownTestSuite()
    {
        // Release callbacks before finalization; finalize only when the
        // test binary created the interpreter.
        rel::Environment::CleanupPythonState();
        python_manager::PyEnvManager::ShutdownPyEnv();
    }
};

TEST_F(PythonPluginTest, IsPythonAvailable)
{
    EXPECT_TRUE(rel::Environment::IsPythonAvailable());
}

// ---------------------------------------------------------------------------
// Function registration: Python -> C++ (register_function + CallFunction)
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, RegisterFunctionAndCallFromCpp)
{
    RUN_PY(
        "import rel\n"
        "def answer(args):\n"
        "    return 42.0\n"
        "rel.register_function('py_answer', [rel.Param('x')], answer)\n");

    rel::Value v =
        rel::Environment::CallFunction("py_answer", rel::Value::Real(0.0));
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 42.0);
}

TEST_F(PythonPluginTest, CallbackReceivesAndTransformsValue)
{
    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "def add_one(args):\n"
        "    return np.asarray(args['x']) + 1.0\n"
        "rel.register_function('py_addone', [rel.Param('x')], add_one)\n");

    rel::Value v =
        rel::Environment::CallFunction("py_addone", rel::Value::Real(41.0));
    ASSERT_TRUE(v.is_measurement());
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 42.0, 1e-12);
}

TEST_F(PythonPluginTest, UnregisterPythonFunction)
{
    RUN_PY(
        "import rel\n"
        "def fn(args):\n"
        "    return 1.0\n"
        "rel.register_function('py_unreg', [rel.Param('x')], fn)\n"
        "rel.unregister_function('py_unreg')\n");

    EXPECT_FALSE(rel::Environment::HasFunction("py_unreg"));
}

// ---------------------------------------------------------------------------
// Builtin functions + constants via the PEP 562 __getattr__ bridge.
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, BuiltinFunctionAndConstantFromPython)
{
    if (!g_numpy_available)
        GTEST_SKIP() << "numpy not installed";

    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "y = rel.sin(rel.Value.real(0.0))\n"
        "assert abs(float(np.asarray(y))) < 1e-12\n"
        "assert abs(float(np.asarray(rel.PI)) - 3.1415926535898) < 1e-9\n");
}

// ---------------------------------------------------------------------------
// numpy interop (buffer protocol)
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, NumpyInterop)
{
    if (!g_numpy_available)
        GTEST_SKIP() << "numpy not installed";

    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "arr = np.array([1.0, 2.0, 3.0, 4.0])\n"
        "ds = rel.DataSeries.from_array(arr)\n"
        "assert len(ds) == 4\n"
        "assert ds.data_type == 'real'\n"
        "assert ds.data_kind == 'scalar'\n"
        "back = np.asarray(ds)\n"
        "assert back.shape == (4,)\n"
        "assert np.allclose(back, arr)\n");
}

TEST_F(PythonPluginTest, MeasurementAndValueNumpy)
{
    if (!g_numpy_available)
        GTEST_SKIP() << "numpy not installed";

    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "m = rel.Measurement(np.array([1.0, 2.0, 3.0]))\n"
        "assert m.data_kind == 'vector'\n"
        "a = np.asarray(m)\n"
        "assert a.shape == (1, 3)   # a vector CELL exports as one row\n"
        "v = rel.Value.array_real([5.0, 6.0, 7.0])\n"
        "av = np.asarray(v)\n"
        "assert av.shape == (3,)    # a 3-row SCALAR series\n");
}

TEST_F(PythonPluginTest, StringAndBooleanExport)
{
    if (!g_numpy_available)
        GTEST_SKIP() << "numpy not installed";

    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "s = rel.Measurement('hello')\n"
        "assert np.asarray(s).dtype.kind == 'U'\n"
        "b = rel.Measurement(True)\n"
        "assert np.asarray(b).dtype == np.bool_\n"
        "vs = rel.Value.array_string(['a', 'b', 'c'])\n"
        "assert list(np.asarray(vs)) == ['a', 'b', 'c']\n");
}

// ---------------------------------------------------------------------------
// DataArray select / at / indep_data (strict C++ semantics)
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, DataArraySelectAndIndepData)
{
    if (!g_numpy_available)
        GTEST_SKIP() << "numpy not installed";

    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "freq = rel.DataSeries.from_array(np.array([1e9, 2e9, 3e9, 4e9]))\n"
        "power = rel.DataSeries.from_array(np.array([-30.0, -20.0, -10.0]))\n"
        "vout = rel.DataSeries.from_array(np.arange(12.0))\n"
        "info = rel.BlockCreateInfo(\n"
        "    independents=[('freq', freq, rel.RegularDim(4)),\n"
        "                  ('power', power, rel.RegularDim(3))],\n"
        "    dependents=[('Vout', vout)])\n"
        "ds = rel.Dataset('d')\n"
        "ds.AddBlock('sim/SP', info)\n"
        "da = ds.GetDataArray('sim/SP', 'Vout')\n"
        "assert da.rank == 2\n"
        "assert da.flat_size == 12\n"
        "assert da.indep_names == ['freq', 'power']\n"
        "sel = da.select([0, 0])\n"
        "assert sel.rank == 1\n"
        "assert sel.flat_size == 1\n"
        "assert len(da.indep_data(1)) == 3\n"
        "assert len(da.indep_data(2)) == 4\n");
}

// ---------------------------------------------------------------------------
// LoadPython from a file
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, LoadPythonFile)
{
    const char* path = "rel_test_load_python_tmp.py";
    {
        std::ofstream f(path);
        f << "import rel\n"
             "def from_file(args):\n"
             "    return 7.0\n"
             "rel.register_function('py_from_file', [rel.Param('x')], from_file)\n";
    }

    bool ok = false;
    try
    {
        ok = rel::Environment::LoadPython(path);
    }
    catch (const std::exception& e)
    {
        FAIL() << "LoadPython error: " << e.what();
    }
    std::remove(path);

    EXPECT_TRUE(ok);
    rel::Value v =
        rel::Environment::CallFunction("py_from_file", rel::Value::Real(0.0));
    ASSERT_TRUE(v.is_measurement());
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 7.0, 1e-12);
}

// ---------------------------------------------------------------------------
// Config-driven plugin loading: Environment::LoadFromConfig + "python_plugins"
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, LoadPluginsFromConfig)
{
    // A self-contained env config in a temp directory: the plugin path in
    // the JSON is relative and must be resolved against the config file's
    // directory.
    const char* dir = "rel_test_cfg_env";
    REL_MKDIR(dir);

    const std::string plugin_path = std::string(dir) + "/cfg_plugin.py";
    {
        std::ofstream f(plugin_path.c_str());
        f << "import rel\n"
             "def cfg_fn(args):\n"
             "    return 3.25\n"
             "rel.register_function('py_cfg_fn', [rel.Param('x')], cfg_fn)\n";
    }

    const std::string config_path = std::string(dir) + "/env.json";
    {
        std::ofstream f(config_path.c_str());
        f << "{\n"
             "  \"python_plugins\": [\"cfg_plugin.py\"]\n"
             "}\n";
    }

    try
    {
        rel::Environment::LoadFromConfig(config_path);
    }
    catch (const std::exception& e)
    {
        std::remove(plugin_path.c_str());
        std::remove(config_path.c_str());
        REL_RMDIR(dir);
        FAIL() << "LoadFromConfig error: " << e.what();
    }

    // The plugin registered its function into the global registry.
    EXPECT_TRUE(rel::Environment::HasFunction("py_cfg_fn"));
    rel::Value v =
        rel::Environment::CallFunction("py_cfg_fn", rel::Value::Real(0.0));
    ASSERT_TRUE(v.is_measurement());
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 3.25, 1e-12);

    // Cleanup.
    rel::Environment::UnregisterFunction("py_cfg_fn");
    std::remove(plugin_path.c_str());
    std::remove(config_path.c_str());
    REL_RMDIR(dir);
}

// ---------------------------------------------------------------------------
// Registration collision: a Python plugin must NOT silently override a
// builtin / C++-registered function.
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, PythonCannotOverrideBuiltinFunction)
{
    // "sin" is a builtin from the math library.
    ASSERT_TRUE(rel::Environment::HasFunction("sin"));

    RUN_PY(
        "import rel\n"
        "def evil(args):\n"
        "    return 0.0\n"
        "try:\n"
        "    rel.register_function('sin', [rel.Param('x')], evil)\n"
        "    raise AssertionError('expected RuntimeError for name collision')\n"
        "except RuntimeError:\n"
        "    pass\n");

    // The builtin must be untouched.
    EXPECT_TRUE(rel::Environment::HasFunction("sin"));
    rel::Value v = rel::Environment::CallFunction(
        "sin", rel::Value::Real(0.0));
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 0.0, 1e-12);
}

// ---------------------------------------------------------------------------
// Vectorized (broadcasting) Python function bodies: the registered function
// returns an ndarray, which round-trips through from_python into a Value.
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, PythonFunctionBroadcastsArrayInput)
{
    // Register a function that squares its input (element-wise).
    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "def sq(args):\n"
        "    return np.asarray(args['x']) ** 2\n"
        "rel.register_function('py_sq', [rel.Param('x')], sq)\n");

    // Vector input -> DataArray result.
    rel::Value v = rel::Environment::CallFunction(
        "py_sq", rel::Value::ArrayReal({1.0, 2.0, 3.0}));
    ASSERT_TRUE(v.is_data_array());
    EXPECT_NEAR(v.as_data_array().datas().begin()->second.scalar_at<double>(0),
                1.0, 1e-12);
    EXPECT_NEAR(v.as_data_array().datas().begin()->second.scalar_at<double>(2),
                9.0, 1e-12);

    // Scalar input -> scalar Measurement result.
    rel::Value s = rel::Environment::CallFunction(
        "py_sq", rel::Value::Real(5.0));
    ASSERT_TRUE(s.is_measurement());
    EXPECT_NEAR(s.as_measurement().as_scalar<double>(), 25.0, 1e-12);

    rel::Environment::UnregisterFunction("py_sq");
}

// ---------------------------------------------------------------------------
// Shape/kind intent preservation: a vector cell that goes through numpy must
// round-trip as a vector (not degrade into an N-row scalar series), while a
// scalar-series input keeps its N-row semantics.
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, PythonFunctionPreservesVectorShape)
{
    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "def ident(args):\n"
        "    return np.asarray(args['x'])\n"
        "rel.register_function('py_ident', [rel.Param('x')], ident)\n");

    // {1,2,3}-shaped input: a single 3-wide VECTOR cell.
    rel::Value v = rel::Environment::CallFunction(
        "py_ident", rel::Value::Vector(
            xdataset::VecXd::LinSpaced(3, 1.0, 3.0).eval()));
    ASSERT_TRUE(v.is_measurement());
    EXPECT_EQ(v.data_kind(), xdataset::DataKind::kVector);
    EXPECT_EQ(v.as_measurement().as_vector<double>().size(), 3u);

    // [1,2,3]-shaped input: an N-row SCALAR series stays N rows.
    rel::Value s = rel::Environment::CallFunction(
        "py_ident", rel::Value::ArrayReal({1.0, 2.0, 3.0}));
    ASSERT_TRUE(s.is_data_array());
    EXPECT_EQ(s.as_data_array().datas().begin()->second.data_kind(),
              xdataset::DataKind::kScalar);
    EXPECT_EQ(s.as_data_array().datas().begin()->second.size(), 3u);

    rel::Environment::UnregisterFunction("py_ident");
}

// ---------------------------------------------------------------------------
//  Language front-end: eval.
//  The embedded module `rel` carries both the value layer (Value,
//  register_function, ...) and the front end (rel.eval).
// ---------------------------------------------------------------------------

TEST_F(PythonPluginTest, RelModuleEval)
{
    RUN_PY(
        "import numpy as np\n"
        "import rel\n"
        "v = rel.eval('1 + 2 * 3')\n"
        "assert float(np.asarray(v)) == 7.0, np.asarray(v)\n");
}

TEST_F(PythonPluginTest, RelModuleEvalErrorRaises)
{
    RUN_PY(
        "import rel\n"
        "try:\n"
        "    rel.eval('1 +')\n"
        "    assert False, 'expected eval error'\n"
        "except RuntimeError as e:\n"
        "    assert 'syntax error' in str(e), str(e)\n");
}

}  // namespace
