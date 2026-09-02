#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "core/equation_value.h"
#include "equation_value_test_utils.h"

#include "environment.h"   // rel::Environment
#include "value.h"          // rel::Value
#include "data_series.h"
#include "dataset.h"

using namespace xequation;

namespace
{

// 构造一个最小 Dataset：block "sim/SP"，独立变量 freq，依赖变量 Vout。
xdataset::Dataset MakeSampleDataset(const std::string &name = "noise")
{
    xdataset::Dataset ds;
    ds.set_name(name);

    xdataset::BlockCreateInfo info;
    info.independent_specs.push_back(
        xdataset::IndependentSpec{"freq", xdataset::DataSeries::CreateScalar<double>(2),
                                  xdataset::DimensionSpec::Regular(2)});
    info.dependent_specs.push_back(
        xdataset::DependentSpec{"Vout", xdataset::DataSeries::CreateScalar<double>(2)});

    ds.AddBlock("sim/SP", std::move(info));
    return ds;
}

} // namespace

TEST(RelEngine, TestParse)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // Parse 只接受表达式：语法校验 + 依赖提取，不做名字绑定。
    auto result = manager.Parse("a + b + c");
    EXPECT_EQ(result.status, ResultStatus::kSuccess);
    EXPECT_THAT(result.dependencies, testing::UnorderedElementsAre("a", "b", "c"));
}

TEST(RelEngine, TestParseExpression)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    auto result = manager.Parse("sin(x) + pi");
    EXPECT_EQ(result.status, ResultStatus::kSuccess);
    // sin 是注册函数、pi 是内置常量，都不是依赖
    EXPECT_THAT(result.dependencies, testing::UnorderedElementsAre("x"));
}

// ---- AST 依赖提取精度（正则做不到的场景）-----------------------------

TEST(RelEngine, TestParseDepsFunctionCallVsMatrixIndex)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();

    // 单段 callee 且注册为函数 -> 函数调用，callee 不是依赖，参数是
    auto r1 = manager.Parse("sin(x) * cos(y)");
    EXPECT_THAT(r1.dependencies, testing::UnorderedElementsAre("x", "y"));

    // 非注册函数的 a(...) -> 矩阵索引，a 是依赖
    auto r2 = manager.Parse("a(1, 2) + 1");
    EXPECT_THAT(r2.dependencies, testing::UnorderedElementsAre("a"));
}

TEST(RelEngine, TestParseDepsAttributeChain)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // 多段路径：收集首段（可能是 equation 名）+ 完整路径（DataArray /
    // block 引用），不收集中间前缀（a.b 不能作为依赖名存在）。
    auto result = manager.Parse("a.b.c");
    EXPECT_THAT(result.dependencies,
                testing::UnorderedElementsAre("a", "a.b.c"));
}

TEST(RelEngine, TestParseDepsSelfReference)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // content 是表达式，读取 x 是依赖。
    auto result = manager.Parse("x + 1");
    EXPECT_EQ(result.status, ResultStatus::kSuccess);
    EXPECT_THAT(result.dependencies, testing::UnorderedElementsAre("x"));
}

TEST(RelEngine, TestParseDepsNoStringMisdetect)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // 字符串字面量里的标识符不应被误捕为依赖。
    auto result = manager.Parse(R"("hello world")");
    EXPECT_EQ(result.status, ResultStatus::kSuccess);
    EXPECT_TRUE(result.dependencies.empty());
}

TEST(RelEngine, TestParseDepsSweepAndIndex)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // 列表/矩阵/索引结构里的引用
    auto r1 = manager.Parse("[a, b, c]");
    EXPECT_THAT(r1.dependencies, testing::UnorderedElementsAre("a", "b", "c"));

    auto r2 = manager.Parse("m[1, 2] * n");
    EXPECT_THAT(r2.dependencies, testing::UnorderedElementsAre("m", "n"));
}

TEST(RelEngine, TestParseDepsComparison)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // == 是表达式运算符：两侧的引用都是依赖
    auto result = manager.Parse("a == b");
    EXPECT_EQ(result.status, ResultStatus::kSuccess);
    EXPECT_THAT(result.dependencies, testing::UnorderedElementsAre("a", "b"));
}

TEST(RelEngine, TestParseInvalid)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    // 语法错误：status=kError + message
    auto result = manager.Parse("[::]");
    EXPECT_EQ(result.status, ResultStatus::kError);
    EXPECT_FALSE(result.message.empty());
}

TEST(RelEngine, TestEquationManager)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();

    ObjectId id_a = manager.AddEquation("a", "1");
    manager.Update();

    EXPECT_FALSE(manager.HasVariable("d"));

    // 逐条添加方程 (b/c/d 依赖 a)
    manager.AddEquation("b", "3");
    manager.AddEquation("c", "5");
    manager.AddEquation("d", "a+b*c");
    manager.Update();

    const EquationValue v = manager.GetVariable("d");
    ASSERT_TRUE(v.HasValue());
    EXPECT_EQ(AsScalar<int>(v), 16);

    // REL 内置常量/函数
    manager.AddEquation("p", "pi");
    manager.UpdateEquation("p");
    const EquationValue p = manager.GetVariable("p");
    ASSERT_TRUE(p.HasValue());
    EXPECT_TRUE(IsRealValue(p));
    EXPECT_NEAR(AsScalar<double>(p), 3.14159265358979, 1e-12);
}

TEST(RelEngine, TestEval)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();
    auto r = manager.Eval("2 + 3 * 4");
    EXPECT_EQ(r.status, ResultStatus::kSuccess);
    EXPECT_EQ(AsScalar<int>(r.value), 14);
}

TEST(RelEngine, TestEnvSetGet)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();

    manager.environment().Define("x", rel::Value::Integer(7));
    EXPECT_TRUE(manager.HasVariable("x"));
    EXPECT_EQ(AsScalar<int>(manager.GetVariable("x")), 7);

    manager.environment().Remove("x");
    EXPECT_FALSE(manager.HasVariable("x"));
    EXPECT_FALSE(manager.GetVariable("x").HasValue());
}
