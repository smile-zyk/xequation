#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "python/python_equation_context.h"
#include "python/python_equation_engine.h"

using namespace xequation;
using namespace xequation::python;

TEST(PythonEquationEngine, TestInit)
{
    PythonEquationEngine::SetPyEnvConfig(PythonEquationEngine::PyEnvConfig());
    PythonEquationEngine::GetInstance();
}

TEST(PythonEquationEngine, TestParse)
{
    auto result = PythonEquationEngine::GetInstance().Parse("e = a + b + c");
    
    EXPECT_EQ(result.size(), 1);
    auto eqn = result[0];
    EXPECT_EQ(eqn.name(), "e");
    EXPECT_THAT(result[0].dependencies(), testing::UnorderedElementsAre("a", "b", "c"));
    EXPECT_EQ(eqn.content(), "e = a + b + c");
}

TEST(PythonEquationEngine, EvalTest)
{
    auto& engine = PythonEquationEngine::GetInstance();
    auto variable_manager = engine.CreateEquationManager();

    variable_manager->AddEquation("a", "1");
    variable_manager->AddEquation("b", "3");
    variable_manager->AddEquation("c", "5");
    variable_manager->AddEquation("d", "a + b * c");
    variable_manager->Update();

    auto v = variable_manager->context()->Get("d");
    auto obj = v.Cast<py::object>();
    EXPECT_EQ(obj.cast<int>(), 16);

    variable_manager->EditEquation("b", "b", "c");
    variable_manager->UpdateEquation("b");
    v = variable_manager->context()->Get("d");
    obj = v.Cast<py::object>();
    EXPECT_EQ(obj.cast<int>(), 26);

    variable_manager->AddEquation("test", "sum([a,b,c,d])");
    variable_manager->UpdateEquation("test");

    v = variable_manager->context()->Get("test");
    obj = v.Cast<py::object>();
    EXPECT_EQ(obj.cast<int>(), 37);

    variable_manager->AddMultipleEquations("from math import*;p=pi");
    variable_manager->Update();
    variable_manager->AddEquation("f", "sin(a*p)");
    variable_manager->UpdateEquation("f");
    v = variable_manager->context()->Get("f");
    obj = v.Cast<py::object>();
    double t = obj.cast<double>();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}