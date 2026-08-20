#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_equation_context.h"
#include "python/python_equation_engine.h"

using namespace xequation;
using namespace xequation::python;

TEST(PythonEquationEngine, TestInit)
{
    // 空配置会让嵌入的 CPython 用 DLL 内置的旧 prefix 找 stdlib 而失败，
    // 必须用构建期注入的 REL_PYTHON_* 路径。config 已在 main() 里设置。
    PythonEquationEngine::GetInstance();
}

TEST(PythonEquationEngine, TestParse)
{
    auto result = PythonEquationEngine::GetInstance().Parse("e = a + b + c", ParseMode::kStatement);
    
    EXPECT_EQ(result.items.size(), 1);
    auto item = result.items[0];
    EXPECT_EQ(item.name, "e");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("a", "b", "c"));
    EXPECT_EQ(item.content, "a + b + c");
}

TEST(PythonEquationEngine, TestEquationManager)
{
    auto& engine = PythonEquationEngine::GetInstance();
    pybind11::gil_scoped_acquire acquire;
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
    auto obj = pybind11::cast(v);
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
    obj = pybind11::cast(v);
    EXPECT_EQ(obj.cast<int>(), 26);

    EquationGroupId id_1 = equation_manager->AddEquationGroup("test=sum([a,b,c,d])");
    equation_manager->UpdateEquation("test");

    v = equation_manager->context().Get("test");
    obj = pybind11::cast(v);
    EXPECT_EQ(obj.cast<int>(), 37);

    auto import_id = equation_manager->AddEquationGroup("from math import sin,pi;p=pi");
    equation_manager->UpdateEquationGroup(import_id);
    equation_manager->AddEquationGroup("f=sin(a*p)");
    equation_manager->UpdateEquation("f");
    v = equation_manager->context().Get("f");
    obj = pybind11::cast(v);
    double t = obj.cast<double>();
    EXPECT_NEAR(t, 0.0, 1e-15);

    auto import_sub_module_id = equation_manager->AddEquationGroup("import os.path");
    equation_manager->UpdateEquationGroup(import_sub_module_id);
    auto path_group_id = equation_manager->AddEquationGroup("path1 = os.path.join('home', 'user', 'documents', 'file.txt')");
    equation_manager->UpdateEquation("path1");
    v = equation_manager->context().Get("path1");
    obj = pybind11::cast(v);
    std::string path = obj.cast<std::string>();
    // MSYS2 的 Python 把 ntpath 的 sep/altsep 对调了（sep='/'），
    // 而标准 Windows CPython 是 sep='\\'，所以这里对分隔符做归一化再比较。
    std::replace(path.begin(), path.end(), '\\', '/');
    EXPECT_EQ(path, "home/user/documents/file.txt");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    // 不能用 scoped_interpreter（默认配置找不到 stdlib 会 terminate），
    // 也不能把配置只放在某个 TEST 里：
    // ctest 用 --gtest_filter 单测运行时不会先跑那个 TEST。
    // 所以统一在 main() 里用 REL python_manager 的默认配置初始化嵌入解释器，
    // 引擎构造（GetInstance）会幂等复用已初始化的解释器。
    python_manager::PyEnvManager::SetDefaultPyEnvConfig();
    PythonEquationEngine::GetInstance();
    return RUN_ALL_TESTS();
}