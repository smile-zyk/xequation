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
