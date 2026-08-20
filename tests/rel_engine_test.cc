#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "core/equation.h"
#include "core/equation_common.h"
#include "rel_engine/rel_equation_engine.h"

using namespace xequation;
using namespace xequation::rel_engine;

TEST(RelEquationEngine, TestParse)
{
    auto result = RelEquationEngine::GetInstance().Parse("e = a + b + c", ParseMode::kStatement);

    EXPECT_EQ(result.items.size(), 1);
    auto item = result.items[0];
    EXPECT_EQ(item.name, "e");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("a", "b", "c"));
    EXPECT_EQ(item.content, "a + b + c");
    EXPECT_EQ(item.type, ItemType::kVariable);
}

TEST(RelEquationEngine, TestParseExpression)
{
    auto result = RelEquationEngine::GetInstance().Parse("sin(x) + pi", ParseMode::kExpression);

    EXPECT_EQ(result.items.size(), 1);
    auto item = result.items[0];
    EXPECT_EQ(item.name, "__expression__");
    EXPECT_EQ(item.type, ItemType::kExpression);
    // sin 是注册函数、pi 是内置常量，都不是依赖
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("x"));
}

// ---- AST 依赖提取精度（正则做不到的场景）-----------------------------

TEST(RelEquationEngine, TestParseDepsFunctionCallVsMatrixIndex)
{
    auto& engine = RelEquationEngine::GetInstance();

    // 单段 callee 且注册为函数 -> 函数调用，callee 不是依赖，参数是
    auto r1 = engine.Parse("sin(x) * cos(y)", ParseMode::kExpression);
    EXPECT_THAT(r1.items[0].dependencies, testing::UnorderedElementsAre("x", "y"));

    // 非注册函数的 a(...) -> 矩阵索引，a 是依赖
    auto r2 = engine.Parse("a(1, 2) + 1", ParseMode::kExpression);
    EXPECT_THAT(r2.items[0].dependencies, testing::UnorderedElementsAre("a"));
}

TEST(RelEquationEngine, TestParseDepsAttributeChain)
{
    // 多段路径收集所有前缀：a.b.c -> a、a.b、a.b.c
    auto result = RelEquationEngine::GetInstance().Parse("a.b.c", ParseMode::kExpression);
    EXPECT_THAT(result.items[0].dependencies,
                testing::UnorderedElementsAre("a", "a.b", "a.b.c"));
}

TEST(RelEquationEngine, TestParseDepsSelfReference)
{
    // x = x + 1：RHS 读取 x，是依赖（与 Python 引擎一致）
    auto result = RelEquationEngine::GetInstance().Parse("x = x + 1", ParseMode::kStatement);
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("x"));
}

TEST(RelEquationEngine, TestParseDepsNoStringMisdetect)
{
    // 字符串/单位后缀里的标识符不应被误捕为依赖
    auto result = RelEquationEngine::GetInstance().Parse(R"("hello world" = x)", ParseMode::kStatement);
    // 字符串字面量不是合法标识符 -> 语法错误（不是依赖误报）
    EXPECT_EQ(result.items[0].status, ResultStatus::kSyntaxError);
}

TEST(RelEquationEngine, TestParseDepsSweepAndIndex)
{
    // 列表/矩阵/索引结构里的引用
    auto r1 = RelEquationEngine::GetInstance().Parse("[a, b, c]", ParseMode::kExpression);
    EXPECT_THAT(r1.items[0].dependencies, testing::UnorderedElementsAre("a", "b", "c"));

    auto r2 = RelEquationEngine::GetInstance().Parse("m[1, 2] * n", ParseMode::kExpression);
    EXPECT_THAT(r2.items[0].dependencies, testing::UnorderedElementsAre("m", "n"));
}

TEST(RelEquationEngine, TestParseDepsComparisonNotAssignment)
{
    // == 不是赋值：是表达式，左侧也是引用
    auto result = RelEquationEngine::GetInstance().Parse("a == b", ParseMode::kStatement);
    EXPECT_EQ(result.items[0].type, ItemType::kExpression);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a", "b"));
}

TEST(RelEquationEngine, TestEquationManager)
{
    auto& engine = RelEquationEngine::GetInstance();
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
    EXPECT_EQ(v.Cast<int>(), 16);

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
    EXPECT_EQ(v.Cast<int>(), 26);

    // REL 内置常量/函数
    EquationGroupId id_1 = equation_manager->AddEquationGroup("p=pi");
    equation_manager->UpdateEquationGroup(id_1);
    v = equation_manager->context().Get("p");
    EXPECT_TRUE(v.IsRelValue());
    EXPECT_TRUE(v.IsReal());
    EXPECT_NEAR(v.Cast<double>(), 3.14159265358979, 1e-12);
}

TEST(RelEquationEngine, TestInterpretEval)
{
    auto& engine = RelEquationEngine::GetInstance();
    auto ctx = engine.CreateContext();

    auto r = engine.Interpret("2 + 3 * 4", ctx.get(), InterpretMode::kEval);
    EXPECT_EQ(r.status, ResultStatus::kSuccess);
    EXPECT_EQ(r.value.Cast<int>(), 14);
}

TEST(RelEquationEngine, TestContextSetGet)
{
    auto& engine = RelEquationEngine::GetInstance();
    auto ctx = engine.CreateContext();

    ctx->Set("x", EquationValue(rel::Value::Integer(7)));
    EXPECT_TRUE(ctx->Contains("x"));
    EXPECT_EQ(ctx->Get("x").Cast<int>(), 7);

    ctx->Remove("x");
    EXPECT_FALSE(ctx->Contains("x"));
    EXPECT_TRUE(ctx->Get("x").IsNull());
}
