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

using namespace xequation;

namespace
{

// 单例全局共享：每个用例开头清理两引擎状态（断开信号连接 + Reset）。
XEquationProxy &Proxy()
{
    auto &proxy = XEquationProxy::GetInstance();
    proxy.python_manager().signals_manager().DisconnectAllEvent();
    proxy.rel_manager().signals_manager().DisconnectAllEvent();
    proxy.python_manager().Reset();
    proxy.rel_manager().Reset();
    return proxy;
}

// 读 Python 引擎的值：EquationValue 内部是 PyObjectRef，用 pybind11 转标量。
int PyInt(xequation::EquationManager &manager, const std::string &name)
{
    pybind11::gil_scoped_acquire acquire;
    return pybind11::cast(manager.context().Get(name)).cast<int>();
}

TEST(EmbedProxySingleton, TestGetInstanceSameObject)
{
    EXPECT_EQ(&XEquationProxy::GetInstance(), &XEquationProxy::GetInstance());
}

TEST(EmbedProxySingleton, TestManagerRouting)
{
    auto &proxy = Proxy();
    EXPECT_EQ(proxy.python_manager().engine_info().name, "Python");
    EXPECT_EQ(proxy.rel_manager().engine_info().name, "REL");
}

TEST(EmbedProxySingleton, TestConcreteEngineContextAccess)
{
    auto &proxy = Proxy();

    // 具体 Engine：进程级单例，与 Manager 的 engine_info 一致
    EXPECT_EQ(xequation::python::PythonEquationEngine::GetInstance().GetEngineInfo().name, "Python");
    EXPECT_EQ(xequation::rel_engine::RelEquationEngine::GetInstance().GetEngineInfo().name, "REL");

    // 具体 Context：dynamic_cast 类型必须匹配（证明 manager 装的是正确的 context）
    auto *py_ctx = dynamic_cast<const xequation::python::PythonEquationContext *>(&proxy.python_manager().context());
    auto *rel_ctx = dynamic_cast<const xequation::rel_engine::RelEquationContext *>(&proxy.rel_manager().context());
    ASSERT_NE(py_ctx, nullptr);
    ASSERT_NE(rel_ctx, nullptr);

    // Context 与 Manager 持有的是同一个对象
    EXPECT_EQ(static_cast<const xequation::EquationContext *>(py_ctx),
              &proxy.python_manager().context());
    EXPECT_EQ(static_cast<const xequation::EquationContext *>(rel_ctx),
              &proxy.rel_manager().context());

    // 具体 Context 可直接写值，与 context().Get 互通
    proxy.rel_manager().context().Set("v", EquationValue(7));
    EXPECT_EQ(proxy.rel_manager().context().Get("v").Cast<int>(), 7);
}

TEST(EmbedProxySingleton, TestRelFlow)
{
    auto &proxy = Proxy();
    auto &manager = proxy.rel_manager();

    EquationGroupId id_0 = manager.AddEquationGroup("a = 1\nb = 3\nc = 5\nd = a + b * c");
    manager.Update();
    EXPECT_EQ(manager.context().Get("d").Cast<int>(), 16);

    manager.EditEquationGroup(id_0, "a = 1\nb = c\nc = 5\nd = a + b * c");
    manager.UpdateEquation("b");
    EXPECT_EQ(manager.context().Get("d").Cast<int>(), 26);

    manager.RemoveEquationGroup(id_0);
    EXPECT_FALSE(manager.IsEquationExist("d"));
}

TEST(EmbedProxySingleton, TestPythonFlow)
{
    auto &proxy = Proxy();
    auto &manager = proxy.python_manager();

    EquationGroupId id_0 = manager.AddEquationGroup("a = 1\nb = 3\nc = 5\nd = a + b * c");
    manager.Update();
    EXPECT_EQ(PyInt(manager, "d"), 16);

    manager.EditEquationGroup(id_0, "a = 1\nb = c\nc = 5\nd = a + b * c");
    manager.UpdateEquation("b");
    EXPECT_EQ(PyInt(manager, "d"), 26);
}

TEST(EmbedProxySingleton, TestEnginesIsolated)
{
    auto &proxy = Proxy();
    auto &rel = proxy.rel_manager();
    auto &python = proxy.python_manager();

    rel.AddEquation("x", "100");
    rel.UpdateEquation("x");
    EXPECT_EQ(rel.context().Get("x").Cast<int>(), 100);

    python.AddEquation("x", "200");
    python.UpdateEquation("x");
    EXPECT_EQ(PyInt(python, "x"), 200);

    // 两引擎同名变量互不影响
    EXPECT_EQ(rel.context().Get("x").Cast<int>(), 100);
}

TEST(EmbedProxySingleton, TestParseEvalExec)
{
    auto &proxy = Proxy();
    auto &manager = proxy.rel_manager();

    auto parse = manager.Parse("e = a + b + c", ParseMode::kStatement);
    ASSERT_EQ(parse.items.size(), 1u);
    EXPECT_EQ(parse.items[0].name, "e");
    EXPECT_THAT(parse.items[0].dependencies, testing::UnorderedElementsAre("a", "b", "c"));

    manager.context().Set("a", EquationValue(2));
    manager.context().Set("b", EquationValue(3));
    auto eval = manager.Eval("a + b");
    EXPECT_EQ(eval.status, ResultStatus::kSuccess);
    EXPECT_EQ(eval.value.Cast<int>(), 5);

    auto exec = manager.Exec("t = a * b");
    EXPECT_EQ(exec.status, ResultStatus::kSuccess);
    EXPECT_EQ(manager.context().Get("t").Cast<int>(), 6);
}

TEST(EmbedProxySingleton, TestSignals)
{
    auto &proxy = Proxy();
    auto &manager = proxy.rel_manager();
    auto &signals = manager.signals_manager();

    int added = 0;
    int updated = 0;
    int removed = 0;
    std::string last_updated_name;
    auto c1 = signals.Connect<EquationEvent::kEquationAdded>([&](const Equation *) { ++added; });
    auto c2 = signals.Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            ++updated;
            last_updated_name = eq->name();
        });
    auto c3 = signals.Connect<EquationEvent::kEquationRemoved>([&](const std::string &) { ++removed; });

    auto id = manager.AddEquationGroup("a = 1\nb = a + 1");
    EXPECT_EQ(added, 2);

    manager.Update();
    EXPECT_GE(updated, 2);
    EXPECT_EQ(last_updated_name, "b");

    manager.RemoveEquationGroup(id);
    EXPECT_EQ(removed, 2);

    // 断开后不再收到通知
    c1.disconnect();
    int before = added;
    manager.AddEquationGroup("z = 1");
    EXPECT_EQ(added, before);
}

TEST(EmbedProxySingleton, TestReentrantCallback)
{
    auto &proxy = Proxy();
    auto &manager = proxy.rel_manager();
    auto &signals = manager.signals_manager();

    bool reentered = false;
    signals.Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation *eq, bitmask::bitmask<EquationUpdateFlag>) {
            if (!reentered && eq->name() == "a")
            {
                reentered = true;
                manager.UpdateEquation("a"); // 回调内重入，不应死锁
            }
        });

    manager.AddEquationGroup("a = 1");
    manager.Update();
    EXPECT_TRUE(reentered);
}

} // namespace

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    // 预热两个引擎（Python 解释器初始化幂等，需在测试开始前就绪）。
    XEquationProxy::GetInstance().python_manager();
    XEquationProxy::GetInstance().rel_manager();
    return RUN_ALL_TESTS();
}
