#include "core/dependency_graph.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace xequation;

// Test basic node operations
TEST(DependencyGraphTest, NodeOperations) {
  DependencyGraph graph;
  
  // Add node
  EXPECT_TRUE(graph.AddNode("A"));
  EXPECT_TRUE(graph.IsNodeExist("A"));
  
  // Adding duplicate should fail
  EXPECT_FALSE(graph.AddNode("A"));
  
  // Remove node
  EXPECT_TRUE(graph.RemoveNode("A"));
  EXPECT_FALSE(graph.IsNodeExist("A"));
  
  // Removing non-existent node should fail
  EXPECT_FALSE(graph.RemoveNode("B"));
}

// Test dangling edge handling
TEST(DependencyGraphTest, DanglingEdges) {
  DependencyGraph graph;
  
  // Add dangling edge (both nodes don't exist)
  DependencyGraph::Edge edgeAB("A", "B");
  EXPECT_TRUE(graph.AddEdge(edgeAB));
  
  // Verify edge exists but no connection relationship
  EXPECT_TRUE(graph.IsEdgeExist(edgeAB));
  EXPECT_EQ(graph.GetNode("A"), nullptr);
  EXPECT_EQ(graph.GetNode("B"), nullptr);
  
  // Only add target node B
  EXPECT_TRUE(graph.AddNode("B"));
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // Verify connection not established (because A doesn't exist)
  EXPECT_TRUE(nodeB->dependents().empty());   // B has no dependents
  EXPECT_TRUE(nodeB->dependencies().empty());  // B has no dependencies
  
  // Add source node A - connection should now be automatically established
  EXPECT_TRUE(graph.AddNode("A"));
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  
  // Verify connection is established
  EXPECT_EQ(nodeA->dependencies().size(), 1);  // A depends on B
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);     // B has A as dependent
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// Test partial dangling edge activation
TEST(DependencyGraphTest, PartialDanglingEdgeActivation) {
  DependencyGraph graph;
  
  // First add node A
  EXPECT_TRUE(graph.AddNode("A"));
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  
  // Add edge AB (B doesn't exist)
  DependencyGraph::Edge edgeAB("A", "B");
  EXPECT_TRUE(graph.AddEdge(edgeAB));
  
  // Verify connection not established
  EXPECT_TRUE(nodeA->dependencies().empty());
  
  // Add node B
  EXPECT_TRUE(graph.AddNode("B"));
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // Verify connection is established
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// Test connection deactivation when node is removed
TEST(DependencyGraphTest, EdgeDeactivationOnNodeRemoval) {
  DependencyGraph graph;
  
  // Add complete connection
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddEdge({"A", "B"});
  
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  
  // Verify connection exists
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(nodeB->dependents().size(), 1);
  
  // Remove node B
  EXPECT_TRUE(graph.RemoveNode("B"));
  
  // Verify connection broken (but edge remains as dangling)
  nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  EXPECT_TRUE(nodeA->dependencies().empty());
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  
  // Re-add node B
  EXPECT_TRUE(graph.AddNode("B"));
  nodeB = graph.GetNode("B");
  ASSERT_NE(nodeB, nullptr);
  
  // Verify connection automatically restored
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
}

// Test topological sorting
TEST(DependencyGraphTest, TopologicalSort) {
  DependencyGraph graph;
  
  // Add nodes
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddNode("C");
  graph.AddNode("D");
  
  // Build dependencies: A -> B -> C, D is independent
  graph.AddEdge({"A", "B"});
  graph.AddEdge({"B", "C"});
  
  auto sorted = graph.TopologicalSort();
  ASSERT_EQ(sorted.size(), 4);
  
  // Verify order: C and D first (no dependencies)
  // In topological sort, nodes without dependencies can be in any order
  bool c_found = false, d_found = false;
  for (size_t i = 0; i < 2; i++) {
    if (sorted[i] == "C") c_found = true;
    if (sorted[i] == "D") d_found = true;
  }
  EXPECT_TRUE(c_found);
  EXPECT_TRUE(d_found);
  
  // Verify dependency order: B after C, A after B
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
  
  // Prepare valid dependency
  graph.AddNode("A");
  graph.AddNode("B");
  graph.AddEdge({"A", "B"});
  
  // Try to create a cycle
  try {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    graph.AddEdge({"B", "A"});  // Forms a cycle
    
    // Should throw exception when committing
    guard.commit();
    FAIL() << "Expected DependencyCycleException";
  } catch (const DependencyCycleException& e) {
    // Verify cycle path information
    const auto& cycle = e.cycle_path();
    
    // Verify basic cycle properties
    ASSERT_GE(cycle.size(), 2);  // At least contains start and end
    EXPECT_EQ(cycle.front(), cycle.back());  // Start and end are the same
    EXPECT_NE(cycle[0], cycle[1]);  // Intermediate nodes are different
    
    // Verify cycle contains A and B
    bool hasA = false, hasB = false;
    for (const auto& node : cycle) {
      if (node == "A") hasA = true;
      if (node == "B") hasB = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
    
    // Verify correct cycle length (A→B→A or B→A→B)
    EXPECT_TRUE(cycle.size() == 2 || cycle.size() == 3);
    
    // If it's a 3-node cycle, verify intermediate nodes are different
    if (cycle.size() == 3) {
      EXPECT_NE(cycle[0], cycle[1]);
      EXPECT_NE(cycle[1], cycle[2]);
    }
  } catch (...) {
    FAIL() << "Expected DependencyCycleException";
  }
  
  // Verify state rollback
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  
  // Original dependency relationship remains unchanged
  EXPECT_EQ(nodeA->dependencies().size(), 1);
  EXPECT_EQ(*nodeA->dependencies().begin(), "B");
  EXPECT_EQ(nodeB->dependents().size(), 1);
  EXPECT_EQ(*nodeB->dependents().begin(), "A");
  
  // Verify illegal edge was not added
  EXPECT_FALSE(graph.IsEdgeExist({"B", "A"}));
}

TEST(DependencyGraphTest, MultipleCyclesDetection) {
  DependencyGraph graph;
  
  // Add nodes
  graph.AddNodes({"A", "B", "C", "D", "E"});
  
  // Try to add complex edge structure (containing two cycles)
  try {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Add edges
    graph.AddEdges({
      {"A", "B"},  // A depends on B
      {"A", "C"},  // A depends on C
      {"B", "C"},  // B depends on C
      {"C", "D"},  // C depends on D
      {"D", "B"},  // D depends on B -> Forms cycle: B→C→D→B
      {"E", "D"},  // E depends on D
      {"B", "E"},  // B depends on E -> Forms cycle: B→E→D→B
    });
    
    // Commit operation, should detect cycle
    guard.commit();
    FAIL() << "Expected DependencyCycleException";
  } catch (const DependencyCycleException& e) {
    // Verify cycle path information
    const auto& cycle = e.cycle_path();
    
    // Verify basic cycle properties
    ASSERT_GE(cycle.size(), 3);
    EXPECT_EQ(cycle.front(), cycle.back());
    
    // Verify cycle contains key nodes (B and D)
    bool hasB = false, hasD = false;
    for (const auto& node : cycle) {
      if (node == "B") hasB = true;
      if (node == "D") hasD = true;
    }
    EXPECT_TRUE(hasB);
    EXPECT_TRUE(hasD);
    
    // Possible cycle paths:
    // Cycle 1: [B, C, D, B]
    // Cycle 2: [B, E, D, B]
    
    // Verify cycle length (should be 3 different nodes + start repetition)
    EXPECT_EQ(cycle.size(), 4);  // 3 different nodes + start repetition
    
    // Verify intermediate nodes
    bool hasC = std::find(cycle.begin(), cycle.end(), "C") != cycle.end();
    bool hasE = std::find(cycle.begin(), cycle.end(), "E") != cycle.end();
    
    // Can only contain one of C or E
    EXPECT_TRUE(hasC ^ hasE);  // XOR operation: either has C or has E, but not both
  } catch (...) {
    FAIL() << "Expected DependencyCycleException";
  }
  
  // Verify all edges were rolled back (no edges added)
  EXPECT_TRUE(graph.GetAllEdges().first == graph.GetAllEdges().second);
  
  // Verify nodes still exist
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // Verify nodes have no dependencies
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

// Test cascading cleanup when node is removed
TEST(DependencyGraphTest, NodeRemovalCleanup) {
  DependencyGraph graph;
  
  graph.AddNode("Parent");
  graph.AddNode("Child1");
  graph.AddNode("Child2");
  graph.AddEdge({"Parent", "Child1"});
  graph.AddEdge({"Parent", "Child2"});
  
  // Verify initial connection
  auto parent = graph.GetNode("Parent");
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->dependencies().size(), 2);
  
  // Remove child node
  EXPECT_TRUE(graph.RemoveNode("Child1"));
  
  // Verify connection updated
  parent = graph.GetNode("Parent");
  ASSERT_NE(parent, nullptr);
  EXPECT_EQ(parent->dependencies().size(), 1);
  EXPECT_EQ(*parent->dependencies().begin(), "Child2");
  
  // Verify dangling edge still exists
  EXPECT_TRUE(graph.IsEdgeExist({"Parent", "Child1"}));
}

// Test batch operation commit and rollback
TEST(DependencyGraphTest, BatchOperations) {
  DependencyGraph graph;
  
  // Successful batch operation commit
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    graph.AddNode("X");
    graph.AddNode("Y");
    graph.AddEdge({"X", "Y"});
    
    guard.commit();
  }
  
  // Verify commit results
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.IsNodeExist("Y"));
  EXPECT_TRUE(graph.IsEdgeExist({"X", "Y"}));
  
  // Create cycle causing rollback
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    graph.AddNode("Z");
    graph.AddEdge({"Y", "Z"});
    graph.AddEdge({"Z", "X"});  // Forms cycle X->Y->Z->X
    
    try {
      guard.commit();
      FAIL() << "Expected DependencyCycleException";
    } catch (const DependencyCycleException&) {
      // Expected exception
    }
  }
  
  // Verify entire batch operation rolled back
  EXPECT_FALSE(graph.IsNodeExist("Z"));
  EXPECT_FALSE(graph.IsEdgeExist({"Y", "Z"}));
  EXPECT_FALSE(graph.IsEdgeExist({"Z", "X"}));
  
  // Original data should be preserved
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.IsNodeExist("Y"));
  EXPECT_TRUE(graph.IsEdgeExist({"X", "Y"}));
}

// Test batch addition of nodes and edges (no cycle)
TEST(DependencyGraphTest, BatchAddNodesEdgesSuccess) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Batch add nodes
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C", "D"}));
    
    // Mixed single node addition
    EXPECT_TRUE(graph.AddNode("E"));
    
    // Batch add edges
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"B", "C"},
      {"C", "D"},
      {"D", "E"}
    };
    EXPECT_TRUE(graph.AddEdges(edges));
    
    // Mixed single edge addition
    EXPECT_TRUE(graph.AddEdge({"A", "D"}));
    
    guard.commit();
  }
  
  // Verify all nodes exist
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // Verify edges exist
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"C", "D"}));
  EXPECT_TRUE(graph.IsEdgeExist({"D", "E"}));
  EXPECT_TRUE(graph.IsEdgeExist({"A", "D"}));
  
  // Verify topological sort (no cycle)
  EXPECT_NO_THROW(graph.TopologicalSort());
}

// Test batch addition of nodes and edges (forming cycle)
TEST(DependencyGraphTest, BatchAddNodesEdgesCycle) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Batch add nodes
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C"}));
    
    // Batch add edges (including edge that forms cycle)
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"B", "C"},
      {"C", "A"}  // This will form a cycle
    };
    EXPECT_TRUE(graph.AddEdges(edges));
    
    // Try to commit, should detect cycle
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // Verify all operations rolled back
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
}

// Test mixed single and batch operations (no cycle)
TEST(DependencyGraphTest, MixedOperationsSuccess) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Single add node
    EXPECT_TRUE(graph.AddNode("A"));
    
    // Batch add nodes
    EXPECT_TRUE(graph.AddNodes({"B", "C"}));
    
    // Single add edge
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));
    
    // Batch add edges
    EXPECT_TRUE(graph.AddEdges({{"B", "C"}, {"A", "C"}}));
    
    // Add independent node
    EXPECT_TRUE(graph.AddNode("D"));
    
    guard.commit();
  }
  
  // Verify all nodes exist
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  
  // Verify edges exist
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"A", "C"}));
  
  // Verify topological sort
  auto sorted = graph.TopologicalSort();
  EXPECT_EQ(sorted.size(), 4);
  
  // Verify dependency relationships
  auto nodeA = graph.GetNode("A");
  auto nodeB = graph.GetNode("B");
  auto nodeC = graph.GetNode("C");
  
  ASSERT_NE(nodeA, nullptr);
  ASSERT_NE(nodeB, nullptr);
  ASSERT_NE(nodeC, nullptr);
  
  EXPECT_EQ(nodeA->dependencies().size(), 2);  // A depends on B and C
  EXPECT_EQ(nodeB->dependencies().size(), 1);  // B depends on C
  EXPECT_TRUE(nodeC->dependencies().empty());  // C has no dependencies
}

// Test mixed single and batch operations (forming cycle)
TEST(DependencyGraphTest, MixedOperationsCycle) {
  DependencyGraph graph;
  
  // Pre-add a node
  graph.AddNode("X");
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Batch add nodes
    EXPECT_TRUE(graph.AddNodes({"A", "B", "C"}));
    
    // Single add edge
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));
    
    // Batch add edges
    EXPECT_TRUE(graph.AddEdges({{"B", "C"}, {"C", "A"}}));  // Forms cycle A→B→C→A
    
    // Add another valid edge
    EXPECT_TRUE(graph.AddEdge({"X", "A"}));
    
    // Try to commit, should detect cycle
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // Verify all batch operations rolled back
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  
  // Verify cycle edges don't exist
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
  
  // Verify valid edge also rolled back
  EXPECT_FALSE(graph.IsEdgeExist({"X", "A"}));
  
  // Verify pre-added node remains unchanged
  EXPECT_TRUE(graph.IsNodeExist("X"));
  EXPECT_TRUE(graph.GetNode("X")->dependencies().empty());
  EXPECT_TRUE(graph.GetNode("X")->dependents().empty());
}

// Test duplicate additions in batch operations
TEST(DependencyGraphTest, BatchDuplicateOperations) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Add nodes (including duplicates)
    EXPECT_FALSE(graph.AddNodes({"A", "A", "B"}));  // Duplicate add A
    EXPECT_FALSE(graph.AddNode("A"));                // Single duplicate add
    EXPECT_TRUE(graph.AddNode("C"));                 // Valid add
    EXPECT_TRUE(graph.AddNode("D"));                 // Add new node to avoid cycle
    
    // Add edges (including duplicates)
    std::vector<DependencyGraph::Edge> edges = {
      {"A", "B"},
      {"A", "B"},  // Duplicate edge
      {"B", "C"}
    };
    EXPECT_FALSE(graph.AddEdges(edges));  // Returns false because of duplicates
    
    // Add single edge (duplicate)
    EXPECT_FALSE(graph.AddEdge({"A", "B"}));
    
    // Add valid edge that won't form cycle
    EXPECT_TRUE(graph.AddEdge({"C", "D"}));  // Won't form cycle
    
    // Commit all operations
    guard.commit();
  }
  
  // Verify final state
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_TRUE(graph.IsNodeExist("B"));
  EXPECT_TRUE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  
  // Verify edges (duplicate addition should only exist once)
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_TRUE(graph.IsEdgeExist({"C", "D"}));
  
  std::vector<std::string> sorted;
  sorted = graph.TopologicalSort();
  
  // Verify sort results
  ASSERT_EQ(sorted.size(), 4);
  
  // Dependency order should be D -> C -> B -> A
  size_t posD = -1, posC = -1, posB = -1, posA = -1;
  for (size_t i = 0; i < sorted.size(); i++) {
    if (sorted[i] == "A") posA = i;
    if (sorted[i] == "B") posB = i;
    if (sorted[i] == "C") posC = i;
    if (sorted[i] == "D") posD = i;
  }
  
  // Verify dependency order
  EXPECT_LT(posD, posC);
  EXPECT_LT(posC, posB);
  EXPECT_LT(posB, posA);
}

// Test partially invalid operations in batch
TEST(DependencyGraphTest, BatchPartialInvalidOperations) {
  DependencyGraph graph;
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Add valid node
    EXPECT_TRUE(graph.AddNode("A"));
    
    // Add edge to non-existent node
    EXPECT_TRUE(graph.AddEdge({"A", "B"}));  // B doesn't exist, but edge will be stored
    
    // Batch add nodes (including valid and invalid)
    EXPECT_FALSE(graph.AddNodes({"B", "C", "B"}));  // Duplicate add B
    
    // Add valid edge
    EXPECT_TRUE(graph.AddEdge({"B", "C"}));
    
    // Add edge that will form cycle
    EXPECT_TRUE(graph.AddEdge({"C", "A"}));  // Forms cycle A→B→C→A
    
    // Try to commit, should detect cycle
    EXPECT_THROW(guard.commit(), DependencyCycleException);
  }
  
  // Verify all operations rolled back
  EXPECT_FALSE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  
  // Verify edges don't exist
  EXPECT_FALSE(graph.IsEdgeExist({"A", "B"}));
  EXPECT_FALSE(graph.IsEdgeExist({"B", "C"}));
  EXPECT_FALSE(graph.IsEdgeExist({"C", "A"}));
}

TEST(DependencyGraphTest, BatchRemoveOperations) {
  DependencyGraph graph;
  
  // Pre-populate data
  graph.AddNodes({"A", "B", "C", "D"});
  graph.AddEdges({
    {"A", "B"},  // A depends on B
    {"B", "C"},  // B depends on C
    {"C", "D"}   // C depends on D
  });
  
  {
    DependencyGraph::BatchUpdateGuard guard(&graph);
    
    // Batch remove nodes
    EXPECT_TRUE(graph.RemoveNodes({"B", "C"}));
    
    // Remove dangling edge (nodes don't exist but edge still exists)
    EXPECT_TRUE(graph.RemoveEdge({"C", "D"}));  // Remove dangling edge
    
    // Add new node and edges
    EXPECT_TRUE(graph.AddNode("E"));
    EXPECT_TRUE(graph.AddEdge({"A", "E"}));  // A depends on E
    EXPECT_TRUE(graph.AddEdge({"E", "D"}));   // E depends on D
    
    guard.commit();
  }
  
  // Verify node status
  EXPECT_TRUE(graph.IsNodeExist("A"));
  EXPECT_FALSE(graph.IsNodeExist("B"));
  EXPECT_FALSE(graph.IsNodeExist("C"));
  EXPECT_TRUE(graph.IsNodeExist("D"));
  EXPECT_TRUE(graph.IsNodeExist("E"));
  
  // Verify edge status - dangling edges still exist until explicitly removed
  EXPECT_TRUE(graph.IsEdgeExist({"A", "B"}));   // Dangling edge (A exists, B doesn't)
  EXPECT_TRUE(graph.IsEdgeExist({"B", "C"}));   // Dangling edge (B and C don't exist)
  EXPECT_FALSE(graph.IsEdgeExist({"C", "D"}));  // Explicitly removed
  EXPECT_TRUE(graph.IsEdgeExist({"A", "E"}));
  EXPECT_TRUE(graph.IsEdgeExist({"E", "D"}));
  
  // Verify connection relationships
  auto nodeA = graph.GetNode("A");
  ASSERT_NE(nodeA, nullptr);
  EXPECT_EQ(nodeA->dependencies().size(), 1);  // Only E, B is dangling edge
  EXPECT_EQ(*nodeA->dependencies().begin(), "E");
  
  // Verify topological sort
  auto sorted = graph.TopologicalSort();
  ASSERT_EQ(sorted.size(), 3);
  
  // Dependency order: D -> E -> A
  EXPECT_EQ(sorted[0], "D");
  EXPECT_EQ(sorted[1], "E");
  EXPECT_EQ(sorted[2], "A");
}

// Test graph reset
TEST(DependencyGraphTest, Reset) {
  DependencyGraph graph;
  
  graph.AddNodes({"X", "Y", "Z"});
  graph.AddEdges({
    {"X", "Y"},
    {"Y", "Z"}
  });
  
  graph.Reset();
  
  // Verify all content cleared
  EXPECT_FALSE(graph.IsNodeExist("X"));
  EXPECT_FALSE(graph.IsNodeExist("Y"));
  EXPECT_FALSE(graph.IsNodeExist("Z"));
  EXPECT_TRUE(graph.GetAllEdges().first == graph.GetAllEdges().second);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}