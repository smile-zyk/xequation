#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "core/equation_manager.h"
#include "core/equation_value.h"
#include "equation_value_test_utils.h"

#include "environment.h"  // rel::Environment
#include "value.h"         // rel::Value

using namespace xequation;

namespace
{

// 单例全局共享：每个用例开头清理引擎状态（断开信号连接 + Reset）。
EquationManager &Proxy()
{
    EquationManager &manager = EquationManager::GetInstance();
    manager.signals_manager().DisconnectAllEvent();
    manager.Reset();
    return manager;
}

TEST(EmbedProxySingleton, TestGetInstanceSameObject)
{
    EXPECT_EQ(&EquationManager::GetInstance(), &EquationManager::GetInstance());
}

TEST(EmbedProxySingleton, TestEnvAccess)
{
    auto &manager = Proxy();

    // manager 直接持有 rel::Environment：宿主可写值并读回。
    manager.env().Define("v", rel::Value::Integer(7));
    EXPECT_TRUE(manager.HasVariable("v"));
    EXPECT_EQ(AsScalar<int>(manager.GetVariable("v")), 7);

    manager.env().Remove("v");
    EXPECT_FALSE(manager.HasVariable("v"));
    EXPECT_FALSE(manager.GetVariable("v").HasValue());
}

TEST(EmbedProxySingleton, TestRelFlow)
{
    auto &manager = Proxy();

    manager.AddEquation("a", "1");
    manager.AddEquation("b", "3");
    manager.AddEquation("c", "5");
    manager.AddEquation("d", "a + b * c");
    manager.Update();
    EXPECT_EQ(AsScalar<int>(manager.GetVariable("d")), 16);

    manager.EditEquation("b", "c");
    manager.UpdateEquation("b");
    EXPECT_EQ(AsScalar<int>(manager.GetVariable("d")), 26);

    manager.RemoveEquation("d");
    EXPECT_FALSE(manager.IsEquationExist("d"));
}

TEST(EmbedProxySingleton, TestParseEval)
{
    auto &manager = Proxy();

    // Parse 只接受表达式：语法校验 + 依赖提取。
    auto parse = manager.Parse("a + b + c");
    EXPECT_EQ(parse.status, ResultStatus::kSuccess);
    EXPECT_THAT(parse.dependencies, testing::UnorderedElementsAre("a", "b", "c"));

    manager.env().Define("a", rel::Value::Integer(2));
    manager.env().Define("b", rel::Value::Integer(3));
    auto eval = manager.Eval("a + b");
    EXPECT_EQ(eval.status, ResultStatus::kSuccess);
    EXPECT_EQ(AsScalar<int>(eval.value), 5);
}

TEST(EmbedProxySingleton, TestSignals)
{
    auto &manager = Proxy();
    auto &signals = manager.signals_manager();

    int added = 0;
    int updated = 0;
    int removed = 0;
    std::string last_updated_name;
    auto c1 = signals.Connect<EquationEvent::kEquationAdded>([&](const Equation *) { ++added; });
    auto c2 = signals.Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            ++updated;
            last_updated_name = eq->name;
        });
    auto c3 = signals.Connect<EquationEvent::kEquationRemoved>([&](const std::string &) { ++removed; });

    manager.AddEquation("a", "1");
    manager.AddEquation("b", "a + 1");
    EXPECT_EQ(added, 2);

    manager.Update();
    EXPECT_GE(updated, 2);
    EXPECT_EQ(last_updated_name, "b");

    manager.RemoveEquation("a");
    EXPECT_GE(removed, 1);

    // 断开后不再收到通知
    c1.disconnect();
    int before = added;
    manager.AddEquation("z", "1");
    EXPECT_EQ(added, before);
}

TEST(EmbedProxySingleton, TestReentrantCallback)
{
    auto &manager = Proxy();
    auto &signals = manager.signals_manager();

    bool reentered = false;
    signals.Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            if (!reentered && eq->name == "a")
            {
                reentered = true;
                manager.UpdateEquation("a"); // 回调内重入，不应死锁
            }
        });

    manager.AddEquation("a", "1");
    manager.Update();
    EXPECT_TRUE(reentered);
}

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    // 预热 REL 引擎（单例构造幂等，需在测试开始前就绪）。
    EquationManager::GetInstance();
    return RUN_ALL_TESTS();
}
