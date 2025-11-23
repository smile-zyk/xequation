#include <gtest/gtest.h>
#include "core/equation.h"
#include "core/equation_group.h"
#include "core/equation_signals_manager.h"

using namespace xequation;

class EquationSignalsManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        manager = std::make_unique<EquationSignalsManager>();
    }

    std::unique_ptr<Equation> CreateMockEquation(const std::string& name)
    {
        return std::unique_ptr<Equation>(new Equation(name, boost::uuids::uuid(), nullptr));
    }

    std::unique_ptr<EquationGroup> CreateMockEquationGroup()
    {
        return std::unique_ptr<EquationGroup>(new EquationGroup(nullptr));
    }

    void TearDown() override
    {
        manager.reset();
    }

    std::unique_ptr<EquationSignalsManager> manager;
};

TEST_F(EquationSignalsManagerTest, EquationAdded)
{
    auto eq = CreateMockEquation("test");
    bool callbackCalled = false;
    const Equation* capturedEq = nullptr;

    auto connection = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation* equation) {
            callbackCalled = true;
            capturedEq = equation;
        });

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedEq, eq.get());
}

TEST_F(EquationSignalsManagerTest, EquationRemoving)
{
    auto eq = CreateMockEquation("test");
    bool callbackCalled = false;
    const Equation* capturedEq = nullptr;

    auto connection = manager->Connect<EquationEvent::kEquationRemoving>(
        [&](const Equation* equation) {
            callbackCalled = true;
            capturedEq = equation;
        });

    manager->Emit<EquationEvent::kEquationRemoving>(eq.get());

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedEq, eq.get());
}

TEST_F(EquationSignalsManagerTest, EquationUpdate)
{
    auto eq = CreateMockEquation("test");
    bool callbackCalled = false;
    const Equation* capturedEq = nullptr;
    bitmask::bitmask<EquationUpdateFlag> capturedFields;

    auto connection = manager->Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation* equation, bitmask::bitmask<EquationUpdateFlag> fields) {
            callbackCalled = true;
            capturedEq = equation;
            capturedFields = fields;
        });

    auto fields = EquationUpdateFlag::kContent | EquationUpdateFlag::kStatus;
    manager->Emit<EquationEvent::kEquationUpdated>(eq.get(), fields);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedEq, eq.get());
    EXPECT_EQ(capturedFields, fields);
}

TEST_F(EquationSignalsManagerTest, EquationGroupAdded)
{
    auto group = CreateMockEquationGroup();
    bool callbackCalled = false;
    const EquationGroup* capturedGroup = nullptr;

    auto connection = manager->Connect<EquationEvent::kEquationGroupAdded>(
        [&](const EquationGroup* equationGroup) {
            callbackCalled = true;
            capturedGroup = equationGroup;
        });

    manager->Emit<EquationEvent::kEquationGroupAdded>(group.get());

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedGroup, group.get());
}

TEST_F(EquationSignalsManagerTest, EquationGroupRemoving)
{
    auto group = CreateMockEquationGroup();
    bool callbackCalled = false;
    const EquationGroup* capturedGroup = nullptr;

    auto connection = manager->Connect<EquationEvent::kEquationGroupRemoving>(
        [&](const EquationGroup* equationGroup) {
            callbackCalled = true;
            capturedGroup = equationGroup;
        });

    manager->Emit<EquationEvent::kEquationGroupRemoving>(group.get());

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedGroup, group.get());
}

TEST_F(EquationSignalsManagerTest, EquationGroupUpdate)
{
    auto group = CreateMockEquationGroup();
    bool callbackCalled = false;
    const EquationGroup* capturedGroup = nullptr;
    bitmask::bitmask<EquationGroupUpdateFlag> capturedFields;

    auto connection = manager->Connect<EquationEvent::kEquationGroupUpdated>(
        [&](const EquationGroup* equationGroup, bitmask::bitmask<EquationGroupUpdateFlag> fields) {
            callbackCalled = true;
            capturedGroup = equationGroup;
            capturedFields = fields;
        });

    auto fields = EquationGroupUpdateFlag::kStatement | EquationGroupUpdateFlag::kEquationCount;
    manager->Emit<EquationEvent::kEquationGroupUpdated>(group.get(), fields);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(capturedGroup, group.get());
    EXPECT_EQ(capturedFields, fields);
}

// 测试多个回调函数
TEST_F(EquationSignalsManagerTest, MultipleCallbacks)
{
    auto eq = CreateMockEquation("test");
    int callbackCount = 0;

    auto conn1 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });

    auto conn2 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });

    auto conn3 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());

    EXPECT_EQ(callbackCount, 3);
}

// 测试作用域连接
TEST_F(EquationSignalsManagerTest, ScopedConnection)
{
    auto eq = CreateMockEquation("test");
    int callbackCount = 0;

    {
        auto scopedConn = manager->ConnectScoped<EquationEvent::kEquationAdded>(
            [&](const Equation*) { callbackCount++; });

        manager->Emit<EquationEvent::kEquationAdded>(eq.get());
        EXPECT_EQ(callbackCount, 1);
    } // scopedConn 超出作用域，自动断开

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    EXPECT_EQ(callbackCount, 1); // 应该没有增加
}

// 测试手动断开连接
TEST_F(EquationSignalsManagerTest, ManualDisconnect)
{
    auto eq = CreateMockEquation("test");
    int callbackCount = 0;

    auto connection = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    EXPECT_EQ(callbackCount, 1);

    manager->Disconnect(connection);
    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    EXPECT_EQ(callbackCount, 1); // 应该没有增加
}

// 测试断开所有连接
TEST_F(EquationSignalsManagerTest, DisconnectAll)
{
    auto eq = CreateMockEquation("test");
    int callbackCount = 0;

    auto conn1 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });
    auto conn2 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { callbackCount++; });

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    EXPECT_EQ(callbackCount, 2);

    manager->DisconnectAll<EquationEvent::kEquationAdded>();
    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    EXPECT_EQ(callbackCount, 2); // 应该没有增加
}

// 测试断开所有事件的所有连接
TEST_F(EquationSignalsManagerTest, DisconnectAllEvents)
{
    auto eq = CreateMockEquation("test");
    auto group = CreateMockEquationGroup();
    int equationCallbackCount = 0;
    int groupCallbackCount = 0;

    auto eqConn = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { equationCallbackCount++; });
    auto groupConn = manager->Connect<EquationEvent::kEquationGroupAdded>(
        [&](const EquationGroup*) { groupCallbackCount++; });

    manager->DisconnectAllEvent();

    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    manager->Emit<EquationEvent::kEquationGroupAdded>(group.get());

    EXPECT_EQ(equationCallbackCount, 0);
    EXPECT_EQ(groupCallbackCount, 0);
}

// 测试空信号检查
TEST_F(EquationSignalsManagerTest, EmptySignal)
{
    EXPECT_TRUE(manager->IsEmpty<EquationEvent::kEquationAdded>());
    
    auto connection = manager->Connect<EquationEvent::kEquationAdded>(
        [](const Equation*) {});
    
    EXPECT_FALSE(manager->IsEmpty<EquationEvent::kEquationAdded>());
    
    manager->Disconnect(connection);
    EXPECT_TRUE(manager->IsEmpty<EquationEvent::kEquationAdded>());
}

// 测试槽数量统计
TEST_F(EquationSignalsManagerTest, NumSlots)
{
    EXPECT_EQ(manager->GetNumSlots<EquationEvent::kEquationAdded>(), 0);
    
    auto conn1 = manager->Connect<EquationEvent::kEquationAdded>(
        [](const Equation*) {});
    EXPECT_EQ(manager->GetNumSlots<EquationEvent::kEquationAdded>(), 1);
    
    auto conn2 = manager->Connect<EquationEvent::kEquationAdded>(
        [](const Equation*) {});
    EXPECT_EQ(manager->GetNumSlots<EquationEvent::kEquationAdded>(), 2);
    
    manager->Disconnect(conn1);
    EXPECT_EQ(manager->GetNumSlots<EquationEvent::kEquationAdded>(), 1);
    
    manager->DisconnectAll<EquationEvent::kEquationAdded>();
    EXPECT_EQ(manager->GetNumSlots<EquationEvent::kEquationAdded>(), 0);
}

// 测试所有事件类型的连接和发射
TEST_F(EquationSignalsManagerTest, AllEventTypes)
{
    auto eq = CreateMockEquation("test");
    auto group = CreateMockEquationGroup();
    int eventCounter = 0;
    
    // 连接所有事件类型
    auto conn1 = manager->Connect<EquationEvent::kEquationAdded>(
        [&](const Equation*) { eventCounter++; });
    auto conn2 = manager->Connect<EquationEvent::kEquationRemoving>(
        [&](const Equation*) { eventCounter++; });
    auto conn3 = manager->Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation*, auto) { eventCounter++; });
    auto conn4 = manager->Connect<EquationEvent::kEquationGroupAdded>(
        [&](const EquationGroup*) { eventCounter++; });
    auto conn5 = manager->Connect<EquationEvent::kEquationGroupRemoving>(
        [&](const EquationGroup*) { eventCounter++; });
    auto conn6 = manager->Connect<EquationEvent::kEquationGroupUpdated>(
        [&](const EquationGroup*, auto) { eventCounter++; });
    
    // 发射所有事件
    manager->Emit<EquationEvent::kEquationAdded>(eq.get());
    manager->Emit<EquationEvent::kEquationRemoving>(eq.get());
    manager->Emit<EquationEvent::kEquationUpdated>(eq.get(), EquationUpdateFlag::kContent);
    manager->Emit<EquationEvent::kEquationGroupAdded>(group.get());
    manager->Emit<EquationEvent::kEquationGroupRemoving>(group.get());
    manager->Emit<EquationEvent::kEquationGroupUpdated>(group.get(), EquationGroupUpdateFlag::kStatement);
    
    EXPECT_EQ(eventCounter, 6);
}

// 测试位掩码功能
TEST_F(EquationSignalsManagerTest, BitmaskOperations)
{
    auto eq = CreateMockEquation("test");
    bitmask::bitmask<EquationUpdateFlag> receivedFields;
    
    auto connection = manager->Connect<EquationEvent::kEquationUpdated>(
        [&](const Equation*, bitmask::bitmask<EquationUpdateFlag> fields) {
            receivedFields = fields;
        });
    
    // 测试单个字段
    manager->Emit<EquationEvent::kEquationUpdated>(eq.get(), EquationUpdateFlag::kContent);
    EXPECT_TRUE(receivedFields & EquationUpdateFlag::kContent);
    EXPECT_FALSE(receivedFields & EquationUpdateFlag::kStatus);
    
    // 测试多个字段组合
    auto multipleFields = EquationUpdateFlag::kContent | EquationUpdateFlag::kStatus | EquationUpdateFlag::kValue;
    manager->Emit<EquationEvent::kEquationUpdated>(eq.get(), multipleFields);
    
    EXPECT_TRUE(receivedFields & EquationUpdateFlag::kContent);
    EXPECT_TRUE(receivedFields & EquationUpdateFlag::kStatus);
    EXPECT_TRUE(receivedFields & EquationUpdateFlag::kValue);
    EXPECT_FALSE(receivedFields & EquationUpdateFlag::kType);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}