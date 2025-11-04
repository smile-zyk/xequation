// #include <gtest/gtest.h>
// #include <map>
// #include <pybind11/cast.h>
// #include <string>

// #include "python/py_expr_context.h"
// #include "python/py_expr_engine.h"

// using namespace xequation;

// TEST(PyExprEngineTest, InitTest)
// {
//     PyExprEngine::SetPyEnvConfig(PyExprEngine::PyEnvConfig());
//     PyExprEngine::GetInstance();
// }

// TEST(PyExprEngineTest, ParseTest)
// {
//     auto result = PyExprEngine::GetInstance().Parse("a + b + c");

//     EXPECT_EQ(result.status, VariableStatus::kParseSuccess);
//     EXPECT_EQ(result.variables.size(), 3);
// }

// TEST(PyExprEngineTest, EvalTest)
// {
//     auto& engine = PyExprEngine::GetInstance();
//     auto variable_manager = engine.CreateVariableManager();

//     variable_manager->SetValue("a", 1);
//     variable_manager->SetValue("b", 3);
//     variable_manager->SetValue("c", 5);
//     variable_manager->SetExpression("d", "a + b * c");
//     variable_manager->Update();

//     auto v = variable_manager->context()->Get("d");
//     auto obj = v.Cast<py::object>();
//     EXPECT_EQ(obj.cast<int>(), 16);

//     variable_manager->SetExpression("b", "c");
//     variable_manager->UpdateVariable("b");
//     v = variable_manager->context()->Get("d");
//     obj = v.Cast<py::object>();
//     EXPECT_EQ(obj.cast<int>(), 26);

//     variable_manager->SetExpression("test", "sum([a,b,c,d])");
//     variable_manager->UpdateVariable("test");

//     v = variable_manager->context()->Get("test");
//     obj = v.Cast<py::object>();
//     EXPECT_EQ(obj.cast<int>(), 37);

//     // variable_manager->SetExpression("f", "math.sin(a*math.pi)");
//     // variable_manager->UpdateVariable("f");
//     // v = variable_manager->context()->Get("f");
//     // obj = v.Cast<py::object>();
//     // double t = obj.cast<double>();
// }

// int main(int argc, char **argv) {
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }