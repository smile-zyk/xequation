#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "core/equation.h"
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
    auto item = result[0];
    EXPECT_EQ(item.name, "e");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("a", "b", "c"));
    EXPECT_EQ(item.content, "a + b + c");
}

TEST(PythonEquationEngine, TestEquationManager)
{
    auto& engine = PythonEquationEngine::GetInstance();
    auto equation_manager = engine.CreateEquationManager();

    EquationGroupId id_0 = equation_manager->AddEquationGroup(
        R"(
a=1
b=3
c=5
d=a+b*c
)"
    );
    equation_manager->Update();

    auto v = equation_manager->context().Get("d");
    auto obj = v.Cast<pybind11::object>();
    EXPECT_EQ(obj.cast<int>(), 16);

    equation_manager->EditEquationGroup(id_0,
        R"(
a=1
b=c
c=5
d=a+b*c
        )"
    );
    equation_manager->UpdateEquation("b");
    v = equation_manager->context().Get("d");
    obj = v.Cast<pybind11::object>();
    EXPECT_EQ(obj.cast<int>(), 26);

    EquationGroupId id_1 = equation_manager->AddEquationGroup("test=sum([a,b,c,d])");
    equation_manager->UpdateEquation("test");

    v = equation_manager->context().Get("test");
    obj = v.Cast<pybind11::object>();
    EXPECT_EQ(obj.cast<int>(), 37);

    auto import_id = equation_manager->AddEquationGroup("from math import*;p=pi");
    equation_manager->UpdateEquationGroup(import_id);
    equation_manager->AddEquationGroup("f=sin(a*p)");
    equation_manager->UpdateEquation("f");
    v = equation_manager->context().Get("f");
    obj = v.Cast<pybind11::object>();
    double t = obj.cast<double>();
    EXPECT_NEAR(t, 0.0, 1e-15);

    auto import_sub_module_id = equation_manager->AddEquationGroup("import os.path");
    equation_manager->UpdateEquationGroup(import_sub_module_id);
    auto path_group_id = equation_manager->AddEquationGroup("path1 = os.path.join('home', 'user', 'documents', 'file.txt')");
    equation_manager->UpdateEquation("path1");
    v = equation_manager->context().Get("path1");
    obj = v.Cast<pybind11::object>();
    std::string path = obj.cast<std::string>();
    EXPECT_EQ(path, R"(home\user\documents\file.txt)");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}