#include <gtest/gtest.h>
#include <pybind11/eval.h>

#include "python/py_expr_engine.h"
#include "python/py_expr_context.h"

using namespace xexprengine;

TEST(PyExprEngineTest, InitTest)
{
    PyExprEngine::SetPyEnvConfig(PyExprEngine::PyEnvConfig());
    PyExprEngine::GetInstance();
}

TEST(PyExprEngineTest, ParseTest)
{
    auto result = PyExprEngine::GetInstance().Parse("a + b + c");

    EXPECT_EQ(result.status, VariableStatus::kParseSuccess);
    EXPECT_EQ(result.variables.size(), 3);
}

TEST(PyExprEngineTest, EvalTest)
{
    auto variable_manager = PyExprEngine::GetInstance().CreateVariableManager();

    variable_manager->SetValue("a", 1);
    variable_manager->SetValue("b", 3);
    variable_manager->SetValue("c", 5);
    variable_manager->SetExpression("d", "a + b * c");
    variable_manager->Update();

    auto v = variable_manager->context()->Get("d");
    auto obj = v.Cast<py::object>();
    EXPECT_EQ(obj.cast<int>(), 16);

    variable_manager->SetExpression("b", "c");
    variable_manager->UpdateVariable("b");
    v = variable_manager->context()->Get("d");
    obj = v.Cast<py::object>();
    EXPECT_EQ(obj.cast<int>(), 26);

    const char* python_code = R"(
import sys
import os

def print_python_config():
    print("Python path configuration:")
    print(f"  PYTHONHOME = {os.environ.get('PYTHONHOME', '(not set)')}")
    print(f"  PYTHONPATH = {os.environ.get('PYTHONPATH', '(not set)')}")
    print(f"  program name = '{sys.argv[0] if len(sys.argv) > 0 else 'python'}'")
    print(f"  isolated = {1 if sys.flags.isolated else 0}")
    print(f"  environment = {0 if sys.flags.ignore_environment else 1}")
    print(f"  user site = {0 if sys.flags.no_user_site else 1}")
    print(f"  import site = {0 if sys.flags.no_site else 1}")
    print(f"  is in build tree = {0}")
    print(f"  stdlib dir = '{sys.prefix}/lib/python{sys.version_info.major}.{sys.version_info.minor}'")
    print(f"  sys._base_executable = '{getattr(sys, '_base_executable', sys.executable)}'")
    print(f"  sys.base_prefix = '{sys.base_prefix}'")
    print(f"  sys.base_exec_prefix = '{sys.base_exec_prefix}'")
    print(f"  sys.executable = '{sys.executable}'")
    print(f"  sys.prefix = '{sys.prefix}'")
    print(f"  sys.exec_prefix = '{sys.exec_prefix}'")
    
    print("  sys.path = [")
    for path in sys.path:
        print(f"    '{path}',")
    print("  ]")

if __name__ == "__main__":
    print_python_config()
)";

    py::exec(python_code);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}