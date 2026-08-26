#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "xequation_proxy.h"
#include "python/value_pybind_converter.h"
#include "python/python_equation_context.h"
#include "python/python_equation_engine.h"
#include "rel_engine/rel_equation_context.h"
#include "rel_engine/rel_equation_engine.h"

namespace
{

// 单例全局共享：每个用例开头断开全部连接并清理两引擎状态。
XEquationProxy &Proxy()
{
    auto &proxy = XEquationProxy::GetInstance();
    proxy.DisconnectAll(Engine::kPython);
    proxy.DisconnectAll(Engine::kRel);
    proxy.manager(Engine::kPython).Reset();
    proxy.manager(Engine::kRel).Reset();
    return proxy;
}

// 读 Python 引擎的值：EquationValue 内部是 PyObjectRef，用 pybind11 转标量。
int PyInt(XEquationProxy &proxy, const std::string &name)
{
    pybind11::gil_scoped_acquire acquire;
    return pybind11::cast(proxy.GetValue(Engine::kPython, name)).cast<int>();
}

TEST(EmbedProxySingleton, TestGetInstanceSameObject)
{
    EXPECT_EQ(&XEquationProxy::GetInstance(), &XEquationProxy::GetInstance());
}

TEST(EmbedProxySingleton, TestManagerRouting)
{
    auto &proxy = Proxy();
    EXPECT_EQ(proxy.manager(Engine::kPython).engine_info().name, "Python");
    EXPECT_EQ(proxy.manager(Engine::kRel).engine_info().name, "REL");
}

TEST(EmbedProxySingleton, TestConcreteEngineContextAccess)
{
    auto &proxy = Proxy();

    // 具体 Engine：进程级单例，与 Manager 的 engine_info 一致
    EXPECT_EQ(proxy.python_engine().GetEngineInfo().name, "Python");
    EXPECT_EQ(proxy.rel_engine().GetEngineInfo().name, "REL");

    // 具体 Context：dynamic_cast 类型必须匹配，否则抛 std::bad_cast
    EXPECT_NO_THROW(proxy.python_context());
    EXPECT_NO_THROW(proxy.rel_context());

    // Context 与 Manager 持有的是同一个对象
    EXPECT_EQ(static_cast<const xequation::EquationContext *>(&proxy.python_context()),
              &proxy.manager(Engine::kPython).context());
    EXPECT_EQ(static_cast<const xequation::EquationContext *>(&proxy.rel_context()),
              &proxy.manager(Engine::kRel).context());

    // 具体 Context 可直接写值，与 GetValue 互通
    proxy.rel_context().Set("v", EquationValue(7));
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "v").Cast<int>(), 7);
}

TEST(EmbedProxySingleton, TestRelFlow)
{
    auto &proxy = Proxy();

    EquationGroupId id_0 = proxy.AddEquationGroup(
        Engine::kRel, "a = 1\nb = 3\nc = 5\nd = a + b * c");
    proxy.Update(Engine::kRel);
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "d").Cast<int>(), 16);

    proxy.EditEquationGroup(
        Engine::kRel, id_0, "a = 1\nb = c\nc = 5\nd = a + b * c");
    proxy.UpdateEquation(Engine::kRel, "b");
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "d").Cast<int>(), 26);

    proxy.RemoveEquationGroup(Engine::kRel, id_0);
    EXPECT_FALSE(proxy.IsEquationExist(Engine::kRel, "d"));
}

TEST(EmbedProxySingleton, TestPythonFlow)
{
    auto &proxy = Proxy();

    EquationGroupId id_0 = proxy.AddEquationGroup(
        Engine::kPython, "a = 1\nb = 3\nc = 5\nd = a + b * c");
    proxy.Update(Engine::kPython);
    EXPECT_EQ(PyInt(proxy, "d"), 16);

    proxy.EditEquationGroup(
        Engine::kPython, id_0, "a = 1\nb = c\nc = 5\nd = a + b * c");
    proxy.UpdateEquation(Engine::kPython, "b");
    EXPECT_EQ(PyInt(proxy, "d"), 26);
}

TEST(EmbedProxySingleton, TestEnginesIsolated)
{
    auto &proxy = Proxy();

    proxy.AddEquation(Engine::kRel, "x", "100");
    proxy.UpdateEquation(Engine::kRel, "x");
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "x").Cast<int>(), 100);

    proxy.AddEquation(Engine::kPython, "x", "200");
    proxy.UpdateEquation(Engine::kPython, "x");
    EXPECT_EQ(PyInt(proxy, "x"), 200);

    // 两引擎同名变量互不影响
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "x").Cast<int>(), 100);
}

TEST(EmbedProxySingleton, TestParseEvalExec)
{
    auto &proxy = Proxy();

    auto parse = proxy.Parse(Engine::kRel, "e = a + b + c", ParseMode::kStatement);
    ASSERT_EQ(parse.items.size(), 1u);
    EXPECT_EQ(parse.items[0].name, "e");
    EXPECT_THAT(parse.items[0].dependencies, testing::UnorderedElementsAre("a", "b", "c"));

    proxy.rel_context().Set("a", EquationValue(2));
    proxy.rel_context().Set("b", EquationValue(3));
    auto eval = proxy.Eval(Engine::kRel, "a + b");
    EXPECT_EQ(eval.status, ResultStatus::kSuccess);
    EXPECT_EQ(eval.value.Cast<int>(), 5);

    auto exec = proxy.Exec(Engine::kRel, "t = a * b");
    EXPECT_EQ(exec.status, ResultStatus::kSuccess);
    EXPECT_EQ(proxy.GetValue(Engine::kRel, "t").Cast<int>(), 6);
}

TEST(EmbedProxySingleton, TestSignals)
{
    auto &proxy = Proxy();

    int added = 0;
    int updated = 0;
    int removed = 0;
    std::string last_updated_name;
    auto c1 = proxy.ConnectEquationAdded(Engine::kRel, [&](const Equation *) { ++added; });
    auto c2 = proxy.ConnectEquationUpdated(
        Engine::kRel, [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            ++updated;
            last_updated_name = eq->name();
        });
    auto c3 = proxy.ConnectEquationRemoved(Engine::kRel, [&](const std::string &) { ++removed; });

    auto id = proxy.AddEquationGroup(Engine::kRel, "a = 1\nb = a + 1");
    EXPECT_EQ(added, 2);

    proxy.Update(Engine::kRel);
    EXPECT_GE(updated, 2);
    EXPECT_EQ(last_updated_name, "b");

    proxy.RemoveEquationGroup(Engine::kRel, id);
    EXPECT_EQ(removed, 2);

    // 断开后不再收到通知
    c1.disconnect();
    int before = added;
    proxy.AddEquationGroup(Engine::kRel, "z = 1");
    EXPECT_EQ(added, before);
}

TEST(EmbedProxySingleton, TestReentrantCallback)
{
    auto &proxy = Proxy();

    bool reentered = false;
    proxy.ConnectEquationUpdated(
        Engine::kRel, [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            if (!reentered && eq->name() == "a")
            {
                reentered = true;
                proxy.UpdateEquation(Engine::kRel, "a"); // 回调内重入，不应死锁
            }
        });

    proxy.AddEquationGroup(Engine::kRel, "a = 1");
    proxy.Update(Engine::kRel);
    EXPECT_TRUE(reentered);
}

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    // 预热两个引擎（Python 解释器初始化幂等，需在测试开始前就绪）。
    XEquationProxy::GetInstance().manager(Engine::kPython);
    XEquationProxy::GetInstance().manager(Engine::kRel);
    return RUN_ALL_TESTS();
}
