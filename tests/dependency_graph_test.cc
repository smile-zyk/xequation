#include "dependency_graph.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace xexprengine;

// 测试基础节点操作
TEST(DependencyGraphTest, NodeOperations) {
  DependencyGraph graph;
  
  // 添加节点
  EXPECT_TRUE(graph.AddNode("A"));
  EXPECT_TRUE(graph.IsNodeExist("A"));
  
  // 重复添加应失败
  EXPECT_FALSE(graph.AddNode("A"));
  
  // 移除节点
  EXPECT_TRUE(graph.RemoveNode("A"));
  EXPECT_FALSE(graph.IsNodeExist("A"));
  
  // 移除不存在的节点应失败
  EXPECT_FALSE(graph.RemoveNode("B"));
}

// 测试悬空边处理
TEST(DependencyGraphTest, DanglingEdges) {
  DependencyGraph graph;
  
  // 添加悬空边（节点都不存在）
  DependencyGraph::Edge edgeAB("A", "B");
  EXPECT_TRUE(graph.AddEdge(edgeAB));
  
  // 验证边存在但无连接关系
  EXPECT_TRUE(graph.IsEdgeExist(edgeAB));
  EXPECT_EQ(graph.GetNode("A"), nullptr);
  EXPECT_EQ(graph.GetNode("B"), nullptr);
  
  // 只添加目标节点B
  EXPECT_TRUE(graph.AddNode("B"));
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // 验证连接关系未建立（因为A不存在）
  EXPECT_TRUE(nodeB->dependents().empty());   // B没有被任何节点依赖
  EXPECT_TRUE(nodeB->dependencies().empty());  // B不依赖任何节点
  
  // 添加源节点A - 此时连接关系应自动建立
  EXPECT_TRUE(graph.AddNode("A"));
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  
  // 验证连接关系已建立
  EXPECT_EQ(nodeA->dependencies().size(), 1);  // A依赖B
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);     // B被A依赖
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// 测试部分悬空边的激活
TEST(DependencyGraphTest, PartialDanglingEdgeActivation) {
  DependencyGraph graph;
  
  // 先添加节点A
  EXPECT_TRUE(graph.AddNode("A"));
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  
  // 添加边AB（B不存在）
  DependencyGraph::Edge edgeAB("A", "B");
  EXPECT_TRUE(graph.AddEdge(edgeAB));
  
  // 验证连接关系未建立
  EXPECT_TRUE(nodeA->dependencies().empty());
  
  // 添加节点B
  EXPECT_TRUE(graph.AddNode("B"));
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // 验证连接关系已建立
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// 测试节点移除时的连接断开
TEST(DependencyGraphTest, EdgeDeactivationOnNodeRemoval) {
  DependencyGraph graph;
  
  // 添加完整连接
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddEdge({"A", "B"});
  
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  
  // 验证连接存在
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(nodeB->dependents().size(), 1);
  
  // 移除节点B
  EXPECT_TRUE(graph.RemoveNode("B"));
  
  // 验证连接断开（但边仍存在为悬空状态）
  nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  EXPECT_TRUE(nodeA->dependencies().empty());
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  
  // 重新添加节点B
  EXPECT_TRUE(graph.AddNode("B"));
  nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // 验证连接自动恢复
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// 测试拓扑排序
TEST(DependencyGraphTest, TopologicalSort) {
  DependencyGraph graph;
  
  // 添加节点
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddNode("C");
  graph.AddNode("D");
  
  // 构建依赖: A -> B -> C, D独立
  graph.AddEdge({"A", "B"});
  graph.AddEdge({"B", "C"});
  
  auto sorted = graph.TopologicalSort();
  ASSERT_EQ(sorted.size(), 4);
  
  // 验证顺序: C和D在最前（无依赖）
  // 拓扑排序中无依赖节点可任意顺序，但依赖顺序必须保持
  bool c_found = false, d_found = false;
  for (size_t i = 0; i < 2; i++) {
    if (sorted[i] == "C") c_found = true;
    if (sorted[i] == "D") d_found = true;
  }
  EXPECT_TRUE(c_found);
  EXPECT_TRUE(d_found);
  
  // 验证依赖顺序: B在C后，A在B后
  size_t a_pos = -1, b_pos = -1, c_pos = -1;
  for (size_t i = 0; i < sorted.size(); i++) {
    if (sorted[i] == "A") a_pos = i;
    if (sorted[i] == "B") b_pos = i;
    if (sorted[i] == "C") c_pos = i;
  }
  
  EXPECT_GT(a_pos, b_pos);
  EXPECT_GT(b_pos, c_pos);
}

TEST(DependencyGraphTest, CycleDetection) {
  DependencyGraph graph;
  
  // 准备有效依赖
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddEdge({"A", "B"});
  
  // 尝试创建环
  try {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    graph.AddEdge({"B", "A"});  // 形成环
    
    // 提交时应抛出异常
    guard.commit();
    FAIL() << "Expected DependencyCycleException";
  } catch (const DependencyCycleException& e) {
    // 验证环路径信息
    const auto& cycle = e.cycle_path();
    
    // 验证环的基本属性
    ASSERT_GE(cycle.size(), 2);  // 至少包含起点和终点
    EXPECT_EQ(cycle.front(), cycle.back());  // 起点和终点相同
    EXPECT_NE(cycle[0], cycle[1]);  // 中间节点不同
    
    // 验证环中包含A和B
    bool hasA = false, hasB = false;
    for (const auto& node : cycle) {
      if (node == "A") hasA = true;
      if (node == "B") hasB = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
    
    // 验证环长度正确（A→B→A 或 B→A→B）
    EXPECT_TRUE(cycle.size() == 2 || cycle.size() == 3);
    
    // 如果是3节点环，验证中间节点不同
    if (cycle.size() == 3) {
      EXPECT_NE(cycle[0], cycle[1]);
      EXPECT_NE(cycle[1], cycle[2]);
    }
  } catch (...) {
    FAIL() << "Expected DependencyCycleException";
  }
  
  // 验证状态回滚
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  
  // 原始依赖关系保持不变
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
  
  // 验证非法边未添加
  EXPECT_FALSE(graph.IsEdgeExist({"B", "A"}));
}

TEST(DependencyGraphTest, MultipleCyclesDetection) {
  DependencyGraph graph;
  
  // 添加节点
  graph.AddNodes({"A", "B", "C", "D", "E"});
  
  // 尝试添加复杂的边结构（包含两个环）
  try {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 添加边
    graph.AddEdges({
      {"A", "B"},  // A依赖B
      {"A", "C"},  // A依赖C
      {"B", "C"},  // B依赖C
      {"C", "D"},  // C依赖D
      {"D", "B"},  // D依赖B → 形成环：B→C→D→B
      {"E", "D"},  // E依赖D
      {"B", "E"},  // B依赖E → 形成环：B→E→D→B
    });
    
    // 提交操作，应检测到环
    guard.commit();
    FAIL() << "Expected DependencyCycleException";
  } catch (const DependencyCycleException& e) {
    // 验证环路径信息
    const auto& cycle = e.cycle_path();
    
    // 验证环的基本属性
    ASSERT_GE(cycle.size(), 3);
    EXPECT_EQ(cycle.front(), cycle.back());
    
    // 验证环中包含关键节点（B和D）
    bool hasB = false, hasD = false;
    for (const auto& node : cycle) {
      if (node == "B") hasB = true;
      if (node == "D") hasD = true;
    }
    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasD);
    
    // 可能的环路径：
    // 环1: [B, C, D, B]
    // 环2: [B, E, D, B]
    
    // 验证环长度（应为3个不同节点+起点重复）
    EXPECT_EQ(cycle.size(), 4);  // 3个不同节点 + 起点重复
    
    // 验证中间节点
    bool hasC = std::find(cycle.begin(), cycle.end(), "C") != cycle.end();
    bool hasE = std::find(cycle.begin(), cycle.end(), "E") != cycle.end();
    
    // 只能包含C或E中的一个
    EXPECT_TRUE(hasC ^ hasE);  // XOR操作：要么有C，要么有E，但不能同时有
  } catch (...) {
    FAIL() << "Expected DependencyCycleException";
  }
  
  // 验证所有边都被回滚（没有添加任何边）
  EXPECT_TRUE(graph.GetAllEdges().first == graph.GetAllEdges().second);
  
  // 验证节点仍然存在
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // 验证节点没有依赖关系
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  auto nodeC = graph.GetNode("C");
  auto nodeD = graph.GetNode("D");
  auto nodeE = graph.GetNode("E");
  
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  ASSERT_NE(nodeC, nullptr);
  ASSERT_NE(nodeD, nullptr);
  ASSERT_NE(nodeE, nullptr);
  
  EXPECT_TRUE(nodeA->dependencies().empty());
  EXPECT_TRUE(nodeB->dependencies().empty());
  EXPECT_TRUE(nodeC->dependencies().empty());
  EXPECT_TRUE(nodeD->dependencies().empty());
  EXPECT_TRUE(nodeE->dependencies().empty());
}

// 测试节点移除时的级联清理
TEST(DependencyGraphTest, NodeRemovalCleanup) {
  DependencyGraph graph;
  
  graph.AddNode("Parent");
  graph.AddNode("Child1");
  graph.AddNode("Child2");
  graph.AddEdge({"Parent", "Child1"});
  graph.AddEdge({"Parent", "Child2"});
  
  // 验证初始连接
  auto parent = graph.GetNode("Parent");
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->dependencies().size(), 2);
  
  // 移除子节点
  EXPECT_TRUE(graph.RemoveNode("Child1"));
  
  // 验证连接更新
  parent = graph.GetNode("Parent");
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->dependencies().size(), 1);
  EXPECT_EQ(*parent->dependencies().begin(), "Child2");
  
  // 验证悬空边仍然存在
  EXPECT_TRUE(graph.IsEdgeExist({"Parent", "Child1"}));
}

// 测试批量操作提交与回滚
TEST(DependencyGraphTest, BatchOperations) {
  DependencyGraph graph;
  
  // 成功提交批量操作
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    graph.AddNode("X");
    graph.AddNode("Y");
    graph.AddEdge({"X", "Y"});
    
    guard.commit();
  }
  
  // 验证提交结果
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.IsNodeExist("Y"));
  EXPECT_TRUE(graph.IsEdgeExist({"X", "Y"}));
  
  // 创建环导致回滚
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    graph.AddNode("Z");
    graph.AddEdge({"Y", "Z"});
    graph.AddEdge({"Z", "X"});  // 形成环 X->Y->Z->X
    
    try {
      guard.commit();
      FAIL() << "Expected DependencyCycleException";
    } catch (const DependencyCycleException&) {
      // 预期异常
    }
  }
  
  // 验证整个批量操作回滚
  EXPECT_FALSE(graph.IsNodeExist("Z"));
  EXPECT_FALSE(graph.IsEdgeExist({"Y", "Z"}));
  EXPECT_FALSE(graph.IsEdgeExist({"Z", "X"}));
  
  // 原始数据应保留
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.IsNodeExist("Y"));
  EXPECT_TRUE(graph.IsEdgeExist({"X", "Y"}));
}

// 测试批量添加节点和边（无环）
TEST(DependencyGraphTest, BatchAddNodesEdgesSuccess) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 批量添加节点
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C", "D"}));
    
    // 混合单个添加节点
    EXPECT_TRUE(graph.AddNode("E"));
    
    // 批量添加边
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"B", "C"},
      {"C", "D"},
      {"D", "E"}
    };
    EXPECT_TRUE(graph.AddEdges(edges));
    
    // 混合单个添加边
    EXPECT_TRUE(graph.AddEdge({"A", "D"}));
    
    guard.commit();
  }
  
  // 验证所有节点存在
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // 验证边存在
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"C", "D"}));
  EXPECT_TRUE(graph.IsEdgeExist({"D", "E"}));
  EXPECT_TRUE(graph.IsEdgeExist({"A", "D"}));
  
  // 验证拓扑排序（无环）
  EXPECT_NO_THROW(graph.TopologicalSort());
}

// 测试批量添加节点和边（形成环）
TEST(DependencyGraphTest, BatchAddNodesEdgesCycle) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 批量添加节点
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C"}));
    
    // 批量添加边（包括形成环的边）
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"B", "C"},
      {"C", "A"}  // 这将形成环
    };
    EXPECT_TRUE(graph.AddEdges(edges));
    
    // 尝试提交，应检测到环
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // 验证所有操作已回滚
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
}

// 测试混合单个和批量操作（无环）
TEST(DependencyGraphTest, MixedOperationsSuccess) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 单个添加节点
    EXPECT_TRUE(graph.AddNode("A"));
    
    // 批量添加节点
    EXPECT_TRUE(graph.AddNodes({"B", "C"}));
    
    // 单个添加边
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));
    
    // 批量添加边
    EXPECT_TRUE(graph.AddEdges({{"B", "C"}, {"A", "C"}}));
    
    // 添加独立节点
    EXPECT_TRUE(graph.AddNode("D"));
    
    guard.commit();
  }
  
  // 验证所有节点存在
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  
  // 验证边存在
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"A", "C"}));
  
  // 验证拓扑排序
  auto sorted = graph.TopologicalSort();
  EXPECT_EQ(sorted.size(), 4);
  
  // 验证依赖关系
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  auto nodeC = graph.GetNode("C");
  
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  ASSERT_NE(nodeC, nullptr);
  
  EXPECT_EQ(nodeA->dependencies().size(), 2);  // A依赖B和C
  EXPECT_EQ(nodeB->dependencies().size(), 1);  // B依赖C
  EXPECT_TRUE(nodeC->dependencies().empty());  // C无依赖
}

// 测试混合单个和批量操作（形成环）
TEST(DependencyGraphTest, MixedOperationsCycle) {
  DependencyGraph graph;
  
  // 预添加一个节点
  graph.AddNode("X");
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 批量添加节点
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C"}));
    
    // 单个添加边
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));
    
    // 批量添加边
    EXPECT_TRUE(graph.AddEdges({{"B", "C"}, {"C", "A"}}));  // 形成环A→B→C→A
    
    // 添加另一个有效边
    EXPECT_TRUE(graph.AddEdge({"X", "A"}));
    
    // 尝试提交，应检测到环
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // 验证所有批量操作已回滚
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  
  // 验证环边不存在
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
  
  // 验证有效边也被回滚
  EXPECT_FALSE(graph.IsEdgeExist({"X", "A"}));
  
  // 验证预添加节点保持不变
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.GetNode("X")->dependencies().empty());
  EXPECT_TRUE(graph.GetNode("X")->dependents().empty());
}

// 测试批量操作中的重复添加
TEST(DependencyGraphTest, BatchDuplicateOperations) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 添加节点（包含重复）
    EXPECT_FALSE(graph.AddNodes({"A", "A", "B"}));  // 重复添加A
    EXPECT_FALSE(graph.AddNode("A"));                // 单个重复添加
    EXPECT_TRUE(graph.AddNode("C"));                 // 有效添加
    EXPECT_TRUE(graph.AddNode("D"));                 // 添加新节点避免环
    
    // 添加边（包含重复）
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"A", "B"},  // 重复边
      {"B", "C"}
    };
    EXPECT_FALSE(graph.AddEdges(edges));  // 返回false因为有重复
    
    // 添加单个边（重复）
    EXPECT_FALSE(graph.AddEdge({"A", "B"}));
    
    // 添加不会形成环的有效边
    EXPECT_TRUE(graph.AddEdge({"C", "D"}));  // 不会形成环
    
    // 提交所有操作
    guard.commit();
  }
  
  // 验证最终状态
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  
  // 验证边（重复添加只应存在一次）
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"C", "D"}));
  
  std::vector<std::string> sorted;
  sorted = graph.TopologicalSort();
  
  // 验证排序结果
  ASSERT_EQ(sorted.size(), 4);
  
  // 依赖顺序应该是 D -> C -> B -> A
  size_t posD = -1, posC = -1, posB = -1, posA = -1;
  for (size_t i = 0; i < sorted.size(); i++) {
    if (sorted[i] == "A") posA = i;
    if (sorted[i] == "B") posB = i;
    if (sorted[i] == "C") posC = i;
    if (sorted[i] == "D") posD = i;
  }
  
  // 验证依赖顺序
  EXPECT_LT(posD, posC);
  EXPECT_LT(posC, posB);
  EXPECT_LT(posB, posA);
}

// 测试批量操作中的部分无效操作
TEST(DependencyGraphTest, BatchPartialInvalidOperations) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 添加有效节点
    EXPECT_TRUE(graph.AddNode("A"));
    
    // 添加边到不存在的节点
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));  // B不存在，但边会被存储
    
    // 批量添加节点（包含有效和无效）
    EXPECT_FALSE(graph.AddNodes({"B", "C", "B"}));  // 重复添加B
    
    // 添加有效边
    EXPECT_TRUE(graph.AddEdge({"B", "C"}));
    
    // 添加会形成环的边
    EXPECT_TRUE(graph.AddEdge({"C", "A"}));  // 形成环A→B→C→A
    
    // 尝试提交，应检测到环
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // 验证所有操作已回滚
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  
  // 验证边不存在
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
}

TEST(DependencyGraphTest, BatchRemoveOperations) {
  DependencyGraph graph;
  
  // 预填充数据
  graph.AddNodes({"A", "B", "C", "D"});
  graph.AddEdges({
    {"A", "B"},  // A依赖B
    {"B", "C"},  // B依赖C
    {"C", "D"}   // C依赖D
  });
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // 批量移除节点
    EXPECT_TRUE(graph.RemoveNodes({"B", "C"}));
    
    // 移除悬空边（节点不存在但边仍然存在）
    EXPECT_TRUE(graph.RemoveEdge({"C", "D"}));  // 移除悬空边
    
    // 添加新节点和边
    EXPECT_TRUE(graph.AddNode("E"));
    EXPECT_TRUE(graph.AddEdge({"A", "E"}));  // A依赖E
    EXPECT_TRUE(graph.AddEdge({"E", "D"}));   // E依赖D
    
    guard.commit();
  }
  
  // 验证节点状态
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // 验证边状态 - 悬空边仍然存在直到显式移除
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));   // 悬空边（A存在，B不存在）
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));   // 悬空边（B和C都不存在）
  EXPECT_FALSE(graph.IsEdgeExist({"C", "D"}));  // 已被显式移除
  EXPECT_TRUE(graph.IsEdgeExist({"A", "E"}));
  EXPECT_TRUE(graph.IsEdgeExist({"E", "D"}));
  
  // 验证连接关系
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  EXPECT_EQ(nodeA->dependencies().size(), 1);  // 只有E，B是悬空边
  EXPECT_EQ(*nodeA->dependencies().begin(), "E");
  
  // 验证拓扑排序
  auto sorted = graph.TopologicalSort();
  ASSERT_EQ(sorted.size(), 3);
  
  // 依赖顺序：D -> E -> A
  EXPECT_EQ(sorted[0], "D");
  EXPECT_EQ(sorted[1], "E");
  EXPECT_EQ(sorted[2], "A");
}

// 测试重置图
TEST(DependencyGraphTest, Reset) {
  DependencyGraph graph;
  
  graph.AddNodes({"X", "Y", "Z"});
  graph.AddEdges({
    {"X", "Y"},
    {"Y", "Z"}
  });
  
  graph.Reset();
  
  // 验证所有内容已清除
  EXPECT_FALSE(graph.IsNodeExist("X"));
  EXPECT_FALSE(graph.IsNodeExist("Y"));
  EXPECT_FALSE(graph.IsNodeExist("Z"));
  EXPECT_TRUE(graph.GetAllEdges().first == graph.GetAllEdges().second);
}