#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "core/equation_value.h"
#include "equation_value_test_utils.h"

#include "environment.h"  // rel::Environment
#include "value.h"         // rel::Value

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/uuid/random_generator.hpp>

#include <string>
#include <utility>
#include <vector>

using namespace xequation;

namespace
{

// 便捷函数：逐条 AddEquation（批量文本入口已删除）。
void AddEquations(EquationManager &mgr,
                  std::initializer_list<std::pair<std::string, std::string>> eqs)
{
    for (const auto &kv : eqs)
    {
        mgr.AddEquation(kv.first, kv.second);
    }
}

int GetInt(const EquationManager &mgr, const std::string &name)
{
    return AsScalar<int>(mgr.GetVariable(name));
}

} // namespace

class EquationManagerTest : public testing::Test
{
  protected:
    EquationManagerTest() : manager_(EquationManager::GetInstance()) {}

    void SetUp() override
    {
        manager_.Reset();
    }

    EquationManager &manager_;
};

TEST_F(EquationManagerTest, EquationAddRemoveEditGet)
{
    ObjectId id_a = manager_.AddEquation("A", "1");
    EXPECT_TRUE(manager_.IsEquationExist("A"));
    EXPECT_TRUE(manager_.IsEquationExist(id_a));
    EXPECT_EQ(manager_.GetEquationIds().size(), 1u);

    const Equation *equation_a = manager_.GetEquation("A");
    ASSERT_NE(equation_a, nullptr);
    EXPECT_EQ(equation_a->name, "A");
    EXPECT_EQ(equation_a->content, "1");
    EXPECT_EQ(equation_a->id, id_a);
    EXPECT_EQ(manager_.GetEquationById(id_a), equation_a);

    // content edit (name unchanged)
    manager_.EditEquation(id_a, "2");
    EXPECT_TRUE(manager_.IsEquationExist("A"));
    EXPECT_EQ(manager_.GetEquation("A")->content, "2");
    EXPECT_EQ(manager_.GetEquation("A")->id, id_a);  // same identity

    // rename: old name gone, new name appears with the SAME id
    ObjectId id_c = manager_.RenameEquation(id_a, "C");
    EXPECT_FALSE(manager_.IsEquationExist("A"));
    EXPECT_TRUE(manager_.IsEquationExist("C"));
    EXPECT_EQ(id_c, id_a);
    EXPECT_EQ(manager_.GetEquation("C")->id, id_a);

    manager_.RemoveEquation("C");
    EXPECT_FALSE(manager_.IsEquationExist("C"));
    EXPECT_TRUE(manager_.GetEquationIds().empty());
}

TEST_F(EquationManagerTest, EquationTagDefaultsAndCustom)
{
    // Default tag is the "Equation" identity.
    manager_.AddEquation("A", "1");
    EXPECT_EQ(manager_.GetEquation("A")->tag, kEquationTagDefault);

    // A Marker identity can be supplied at creation time.
    manager_.AddEquation("B", "2", kMarkerTagDefault);
    EXPECT_EQ(manager_.GetEquation("B")->tag, kMarkerTagDefault);

    // A custom free-form tag is also accepted.
    manager_.AddEquation("C", "3", "SimulationResult");
    EXPECT_EQ(manager_.GetEquation("C")->tag, "SimulationResult");

    // Edit / rename keep the tag (identity does not change).
    manager_.EditEquation(manager_.GetEquation("C")->id, "4");
    EXPECT_EQ(manager_.GetEquation("C")->tag, "SimulationResult");
    manager_.RenameEquation(manager_.GetEquation("C")->id, "D");
    EXPECT_EQ(manager_.GetEquation("D")->tag, "SimulationResult");
}

TEST_F(EquationManagerTest, ExpressionTagDefaultsAndCustom)
{
    // Default tag is the "Watch" identity.
    const ObjectId watch = manager_.AddExpression("1+1");
    EXPECT_EQ(manager_.GetExpression(watch)->tag, kWatchTagDefault);

    // Graph identity at creation time.
    const ObjectId graph = manager_.AddExpression("1+2", kGraphTagDefault);
    EXPECT_EQ(manager_.GetExpression(graph)->tag, kGraphTagDefault);

    // Custom free-form tag.
    const ObjectId custom = manager_.AddExpression("1+3", "Plotted");
    EXPECT_EQ(manager_.GetExpression(custom)->tag, "Plotted");
}

TEST_F(EquationManagerTest, ExpressionAddRemoveSignals)
{
    int added = 0;
    int removing = 0;
    std::string removed_id;
    EquationManager &mgr = manager_;
    ScopedConnection c_added = mgr.signals_manager().ConnectScoped<EquationEvent::kExpressionAdded>(
        [&](const Expression *) { ++added; });
    ScopedConnection c_removing = mgr.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoving>(
        [&](const Expression *) { ++removing; });
    ScopedConnection c_removed = mgr.signals_manager().ConnectScoped<EquationEvent::kExpressionRemoved>(
        [&](const std::string &id) { removed_id = id; });

    const ObjectId expr_id = manager_.AddExpression("x*2", kGraphTagDefault);
    EXPECT_EQ(added, 1);

    manager_.RemoveExpression(expr_id);
    EXPECT_EQ(removing, 1);
    EXPECT_FALSE(removed_id.empty());

    // Removing a non-existent expression is a no-op: no extra signal.
    manager_.RemoveExpression(expr_id);
    EXPECT_EQ(removing, 1);
}

TEST_F(EquationManagerTest, EquationException)
{
    manager_.AddEquation("A", "1");
    manager_.AddEquation("B", "2");

    // duplicate add
    try
    {
        manager_.AddEquation("A", "3");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationAlreadyExists);
        EXPECT_EQ(e.equation_name(), "A");
    }

    // edit missing
    const ObjectId missing_id = boost::uuids::random_generator()();
    try
    {
        manager_.EditEquation(missing_id, "1");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationNotFound);
        EXPECT_EQ(e.id(), missing_id);
    }

    // rename missing
    try
    {
        manager_.RenameEquation(missing_id, "Z");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationNotFound);
        EXPECT_EQ(e.id(), missing_id);
    }

    // rename onto an existing name
    const ObjectId id_a = manager_.GetEquation("A")->id;
    try
    {
        manager_.RenameEquation(id_a, "B");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationAlreadyExists);
        EXPECT_EQ(e.equation_name(), "B");
    }

    // remove missing -> no-op, no throw
    manager_.RemoveEquation("Z");
    EXPECT_FALSE(manager_.IsEquationExist("Z"));

    // update missing
    try
    {
        manager_.UpdateEquation("E");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationNotFound);
    }
}

TEST_F(EquationManagerTest, EquationManagerUpdate)
{
    AddEquations(manager_, {{"A", "B+C"}, {"B", "D+E"}, {"C", "F"}, {"D", "1"}, {"F", "10"}});
    manager_.AddEquation("E", "5");
    manager_.Update();

    EXPECT_TRUE(manager_.HasVariable("A"));
    EXPECT_TRUE(manager_.HasVariable("B"));
    EXPECT_TRUE(manager_.HasVariable("C"));
    EXPECT_TRUE(manager_.HasVariable("D"));
    EXPECT_TRUE(manager_.HasVariable("E"));
    EXPECT_TRUE(manager_.HasVariable("F"));
    EXPECT_EQ(GetInt(manager_, "A"), 16);
    EXPECT_EQ(GetInt(manager_, "B"), 6);
    EXPECT_EQ(GetInt(manager_, "C"), 10);
    EXPECT_EQ(GetInt(manager_, "D"), 1);
    EXPECT_EQ(GetInt(manager_, "E"), 5);
    EXPECT_EQ(GetInt(manager_, "F"), 10);

    // remove D (A/B lose a dependency)
    manager_.RemoveEquation("D");
    manager_.Update();
    EXPECT_FALSE(manager_.HasVariable("A"));
    EXPECT_FALSE(manager_.HasVariable("B"));
    EXPECT_TRUE(manager_.HasVariable("C"));
    EXPECT_TRUE(manager_.HasVariable("E"));
    EXPECT_TRUE(manager_.HasVariable("F"));
    EXPECT_EQ(manager_.GetEquation("A")->status, ResultStatus::kError);
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kError);

    // re-add D=E and update it: dependents must recover
    manager_.AddEquation("D", "E");
    manager_.UpdateEquation("D");
    EXPECT_TRUE(manager_.HasVariable("A"));
    EXPECT_TRUE(manager_.HasVariable("B"));
    EXPECT_TRUE(manager_.HasVariable("C"));
    EXPECT_TRUE(manager_.HasVariable("D"));
    EXPECT_EQ(GetInt(manager_, "A"), 20);
    EXPECT_EQ(GetInt(manager_, "B"), 10);
    EXPECT_EQ(GetInt(manager_, "C"), 10);
    EXPECT_EQ(GetInt(manager_, "D"), 5);

    // edit C to depend on E too
    manager_.EditEquation(manager_.GetEquation("C")->id, "E+F");
    manager_.UpdateEquation("C");
    EXPECT_EQ(GetInt(manager_, "A"), 25);
    EXPECT_EQ(GetInt(manager_, "B"), 10);
    EXPECT_EQ(GetInt(manager_, "C"), 15);
}

TEST_F(EquationManagerTest, Eval)
{
    AddEquations(manager_, {{"A", "B+C"}, {"B", "D+E"}, {"C", "F"}, {"D", "1"}, {"E", "5"}, {"F", "10"}});
    manager_.Update();
    InterpretResult res = manager_.Eval("A+B");
    EXPECT_EQ(res.status, ResultStatus::kSuccess);
    EXPECT_EQ(AsScalar<int>(res.value), 22);

    res = manager_.Eval("G+1");
    EXPECT_FALSE(res.value.HasValue());
    EXPECT_EQ(res.status, ResultStatus::kError);
}

TEST_F(EquationManagerTest, RenameDependencyDoesNotUpdateDependent)
{
    const ObjectId id_a = manager_.AddEquation("A", "1");
    manager_.AddEquation("B", "A");
    manager_.Update();

    EXPECT_TRUE(manager_.HasVariable("B"));
    EXPECT_EQ(GetInt(manager_, "B"), 1);
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kSuccess);

    // Rename A=1 to C=1.
    manager_.RenameEquation(id_a, "C");

    // "B"'s dependency "A" is gone, so "B" must be dirty.
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());

    manager_.UpdateEquation("C");

    EXPECT_FALSE(manager_.HasVariable("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kError);
    EXPECT_TRUE(manager_.HasVariable("C"));
    EXPECT_EQ(GetInt(manager_, "C"), 1);
}

TEST_F(EquationManagerTest, UpdateEquationAfterRenameRecomputesDirtyDependents)
{
    const ObjectId id_a = manager_.AddEquation("A", "1");
    manager_.AddEquation("B", "A");
    manager_.Update();

    EXPECT_EQ(GetInt(manager_, "B"), 1);

    manager_.RenameEquation(id_a, "C");
    manager_.UpdateEquation("C");

    EXPECT_FALSE(manager_.HasVariable("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kError);
    EXPECT_TRUE(manager_.HasVariable("C"));
    EXPECT_EQ(GetInt(manager_, "C"), 1);
}

TEST_F(EquationManagerTest, ResetContextThenUpdateRecoversAllValues)
{
    AddEquations(manager_, {{"A", "B+C"}, {"B", "D+E"}, {"C", "F"}, {"D", "1"}, {"E", "5"}, {"F", "10"}});
    manager_.Update();

    EXPECT_EQ(GetInt(manager_, "A"), 16);
    EXPECT_EQ(GetInt(manager_, "B"), 6);
    EXPECT_EQ(GetInt(manager_, "C"), 10);

    // After a successful recompute the nodes should be clean.
    EXPECT_FALSE(manager_.graph().GetNode("A")->dirty_flag());
    EXPECT_FALSE(manager_.graph().GetNode("B")->dirty_flag());

    manager_.ResetContext();

    // The context is cleared and every node should be re-dirtied.
    EXPECT_FALSE(manager_.HasVariable("A"));
    EXPECT_TRUE(manager_.graph().GetNode("A")->dirty_flag());
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());
    EXPECT_TRUE(manager_.graph().GetNode("F")->dirty_flag());

    manager_.Update();

    // Everything is restored.
    EXPECT_EQ(GetInt(manager_, "A"), 16);
    EXPECT_EQ(GetInt(manager_, "B"), 6);
    EXPECT_EQ(GetInt(manager_, "C"), 10);
    EXPECT_EQ(GetInt(manager_, "D"), 1);
    EXPECT_EQ(GetInt(manager_, "E"), 5);
    EXPECT_EQ(GetInt(manager_, "F"), 10);
    EXPECT_EQ(manager_.GetEquation("A")->status, ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, UpdateEquationStatusThenUpdateRecovers)
{
    AddEquations(manager_, {{"A", "1"}, {"B", "A"}});
    manager_.Update();

    EXPECT_EQ(GetInt(manager_, "B"), 1);

    // Simulate an interruption: "B" is marked Error and removed from env.
    manager_.UpdateEquationStatus("B", ResultStatus::kError);
    EXPECT_FALSE(manager_.HasVariable("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kError);
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());

    // A later Update must recompute "B".
    manager_.Update();
    EXPECT_EQ(GetInt(manager_, "B"), 1);
    EXPECT_EQ(manager_.GetEquation("B")->status, ResultStatus::kSuccess);
    EXPECT_FALSE(manager_.graph().GetNode("B")->dirty_flag());
}

// ============================================================================
// Registered expressions (只算不存)
// ============================================================================

TEST_F(EquationManagerTest, ExpressionRegisterAndEvaluate)
{
    AddEquations(manager_, {{"A", "1"}, {"B", "2"}});
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("A+B");
    EXPECT_TRUE(manager_.IsExpressionExist(expr_id));
    EXPECT_EQ(manager_.GetExpressionIds().size(), 1u);

    const Expression *expr = manager_.GetExpression(expr_id);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->content, "A+B");
    EXPECT_THAT(expr->dependencies, testing::UnorderedElementsAre("A", "B"));

    // Not evaluated yet.
    EXPECT_FALSE(manager_.GetExpressionValue(expr_id).HasValue());

    manager_.UpdateExpression(expr_id);
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 3);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);

    // The expression is not a symbol: only the two equations A, B are listed.
    EXPECT_THAT(manager_.GetEquationNames(), ::testing::ElementsAre("A", "B"));
    EXPECT_FALSE(manager_.HasVariable("A+B"));
}

TEST_F(EquationManagerTest, ExpressionRecomputesWhenEquationChanges)
{
    manager_.AddEquation("A", "1");
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("A*2");
    manager_.Update();
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 2);

    manager_.EditEquation(manager_.GetEquation("A")->id, "5");
    manager_.Update();
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 10);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, ExpressionRecomputesOnUpdateEquation)
{
    AddEquations(manager_, {{"A", "1"}, {"H", "A"}});
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("H*2");
    manager_.Update();
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 2);

    manager_.EditEquation(manager_.GetEquation("A")->id, "7");
    manager_.UpdateEquation("A");
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 14);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, ExpressionFailureStaysDirtyThenRecovers)
{
    // "X" is undefined at registration: the expression fails and stays dirty.
    // Defining X and updating must recover it.
    ObjectId expr_id = manager_.AddExpression("X+1");
    const Expression *expr = manager_.GetExpression(expr_id);
    ASSERT_NE(expr, nullptr);

    manager_.Update();
    EXPECT_EQ(expr->result.status, ResultStatus::kError);

    std::string expr_node_name;
    manager_.graph().Traversal([&](const std::string &node_name) {
        if (!manager_.IsEquationExist(node_name))
        {
            expr_node_name = node_name;
        }
    });
    ASSERT_FALSE(expr_node_name.empty());
    EXPECT_TRUE(manager_.graph().GetNode(expr_node_name)->dirty_flag());

    manager_.AddEquation("X", "2");
    manager_.Update();
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 3);
    EXPECT_FALSE(manager_.graph().GetNode(expr_node_name)->dirty_flag());
}

TEST_F(EquationManagerTest, ExpressionRemove)
{
    ObjectId expr_id = manager_.AddExpression("A+1");
    EXPECT_TRUE(manager_.IsExpressionExist(expr_id));
    EXPECT_EQ(manager_.GetExpressionIds().size(), 1u);

    manager_.RemoveExpression(expr_id);
    EXPECT_FALSE(manager_.IsExpressionExist(expr_id));
    EXPECT_TRUE(manager_.GetExpressionIds().empty());
    EXPECT_TRUE(manager_.graph().TopologicalSort().empty());
}

TEST_F(EquationManagerTest, UpdateExpressionNotFoundThrows)
{
    ObjectId bogus;  // default-constructed (nil) -> not registered
    EXPECT_THROW(manager_.UpdateExpression(bogus), EquationException);
}

// ============================================================================
// External input symbols (外部输入)
// ============================================================================

TEST_F(EquationManagerTest, ExternalInputRegisterConflicts)
{
    EXPECT_TRUE(manager_.AddExternalInput("c"));
    EXPECT_TRUE(manager_.IsExternalInput("c"));
    EXPECT_THAT(manager_.GetExternalInputNames(), testing::Contains("c"));

    EXPECT_FALSE(manager_.AddExternalInput("c"));
    manager_.AddEquation("a", "1");
    EXPECT_FALSE(manager_.AddExternalInput("a"));

    manager_.RemoveExternalInput("c");
    EXPECT_FALSE(manager_.IsExternalInput("c"));
    EXPECT_THAT(manager_.GetExternalInputNames(), testing::Not(testing::Contains("c")));
}

TEST_F(EquationManagerTest, ExternalInputRegisteredBeforeEquation)
{
    EXPECT_TRUE(manager_.AddExternalInput("c"));
    manager_.AddEquation("x", "c*2");
    manager_.Update();

    // "c" is not in the env -> x is an error until the host provides a value.
    EXPECT_EQ(manager_.GetEquation("x")->status, ResultStatus::kError);

    // The host owns the value (env Define); InvalidateExternalInputs marks the
    // input dirty and recomputes the dependents immediately.
    manager_.environment().Define("c", rel::Value::Integer(3));
    manager_.InvalidateExternalInputs({"c"});
    EXPECT_EQ(GetInt(manager_, "x"), 6);
}

TEST_F(EquationManagerTest, ExternalInputRegisteredAfterEquation)
{
    manager_.AddEquation("x", "c*2");
    EXPECT_TRUE(manager_.AddExternalInput("c"));
    manager_.Update();

    EXPECT_EQ(manager_.GetEquation("x")->status, ResultStatus::kError);

    manager_.environment().Define("c", rel::Value::Integer(4));
    manager_.InvalidateExternalInputs({"c"});
    EXPECT_EQ(GetInt(manager_, "x"), 8);
}

TEST_F(EquationManagerTest, ExternalInputUnknownNameIsIgnored)
{
    manager_.AddExternalInput("c");
    manager_.AddEquation("x", "c+1");
    manager_.environment().Define("c", rel::Value::Integer(10));
    manager_.Update();
    EXPECT_EQ(GetInt(manager_, "x"), 11);

    // A name with no graph node is ignored: it neither invalidates anything
    // nor adds update work.  With no pending dirty nodes, nothing is
    // recomputed -- x keeps its previous value even though "c" changed.
    manager_.environment().Define("c", rel::Value::Integer(20));
    manager_.InvalidateExternalInputs({"bogus"});
    EXPECT_EQ(GetInt(manager_, "x"), 11);  // unchanged

    // Invalidating the real input recomputes the dependent with the new value.
    manager_.InvalidateExternalInputs({"c"});
    EXPECT_EQ(GetInt(manager_, "x"), 21);
}

TEST_F(EquationManagerTest, ExternalInputPropagatesToExpressions)
{
    manager_.AddExternalInput("c");
    manager_.AddEquation("x", "c+1");
    ObjectId expr_id = manager_.AddExpression("x*2");
    manager_.Update();
    EXPECT_EQ(manager_.GetEquation("x")->status, ResultStatus::kError);

    manager_.environment().Define("c", rel::Value::Integer(4));
    manager_.InvalidateExternalInputs({"c"});
    EXPECT_EQ(GetInt(manager_, "x"), 5);
    EXPECT_EQ(AsScalar<int>(manager_.GetExpressionValue(expr_id)), 10);
}
