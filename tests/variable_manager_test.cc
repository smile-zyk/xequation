#include "core/dependency_graph.h"
#include "core/expr_common.h"
#include "core/expr_context.h"
#include "core/variable.h"
#include "core/variable_manager.h"

#include <iterator>
#include <memory>
#include <regex>
#include <string>

#include <gtest/gtest.h>
#include <unordered_set>

using namespace xexprengine;

ParseResult ParseExpr(const std::string &expr)
{
    ParseResult result;
    std::regex expr_regex(R"(^\s*(([A-Z]+|\d+)(\s*([\+\-\*\/])\s*([A-Z]+|\d+))?)\s*$)");
    std::smatch match;
    if (std::regex_match(expr, match, expr_regex))
    {
        // extract variables and operator
        if (std::regex_match(match[2].str(), std::regex(R"(^[A-Z]+$)")))
        {
            result.variables.insert(match[2]);
        }
        if (std::regex_match(match[5].str(), std::regex(R"(^[A-Z]+$)")))
        {
            result.variables.insert(match[5]);
        }
        result.status = xexprengine::VariableStatus::kParseSuccess;
    }
    else
    {
        result.status = xexprengine::VariableStatus::kParseSyntaxError;
    }
    return result;
}

EvalResult EvalExpr(const std::string &expr, const ExprContext *context)
{
    EvalResult result;
    std::regex expr_regex(R"(^\s*(([A-Z]+|\d+)(\s*([\+\-\*\/])\s*([A-Z]+|\d+))?)\s*$)");
    std::smatch match;

    if (std::regex_match(expr, match, expr_regex))
    {
        std::string var1 = match[2];
        int val1 = 0;

        if (std::regex_match(var1, std::regex(R"(^\d+$)")))
        {
            val1 = std::stoi(var1);
        }
        else if (context->Contains(var1))
        {
            Value v1 = context->Get(var1);
            if (v1.Type() == typeid(int))
            {
                val1 = v1.Cast<int>();
            }
            else
            {
                result.status = xexprengine::VariableStatus::kExprEvalTypeError;
                result.eval_error_message = "Variable " + var1 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = xexprengine::VariableStatus::kExprEvalNameError;
            result.eval_error_message = "Variable " + var1 + " not found";
            return result;
        }

        if (!match[4].matched)
        {
            result.status = xexprengine::VariableStatus::kExprEvalSuccess;
            result.value = Value(val1);
            return result;
        }

        std::string op = match[4];
        std::string var2 = match[5];
        int val2 = 0;

        if (std::regex_match(var2, std::regex(R"(^\d+$)")))
        {
            val2 = std::stoi(var2);
        }
        else if (context->Contains(var2))
        {
            Value v2 = context->Get(var2);
            if (v2.Type() == typeid(int))
            {
                val2 = v2.Cast<int>();
            }
            else
            {
                result.status = xexprengine::VariableStatus::kExprEvalTypeError;
                result.eval_error_message = "Variable " + var2 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = xexprengine::VariableStatus::kExprEvalNameError;
            result.eval_error_message = "Variable " + var2 + " not found";
            return result;
        }

        if (op == "+")
        {
            result.status = xexprengine::VariableStatus::kExprEvalSuccess;
            result.value = Value(val1 + val2);
        }
        else if (op == "-")
        {
            result.status = xexprengine::VariableStatus::kExprEvalSuccess;
            result.value = Value(val1 - val2);
        }
        else if (op == "*")
        {
            result.status = xexprengine::VariableStatus::kExprEvalSuccess;
            result.value = Value(val1 * val2);
        }
        else if (op == "/")
        {
            if (val2 == 0)
            {
                result.status = xexprengine::VariableStatus::kExprEvalZeroDivisionError;
            }
            else
            {
                result.status = xexprengine::VariableStatus::kExprEvalSuccess;
                result.value = Value(val1 / val2);
            }
        }
        else
        {
            result.status = xexprengine::VariableStatus::kExprEvalAttributeError;
            result.eval_error_message = "Invalid operator: " + op;
        }
    }
    else
    {
        result.status = xexprengine::VariableStatus::kExprEvalSyntaxError;
    }
    return result;
}

//
class MockExprContext : public ExprContext
{
  public:
    virtual Value Get(const std::string &var_name) const override
    {
        if (Contains(var_name))
        {
            return manager_.at(var_name);
        }
        return Value::Null();
    }

    virtual void Set(const std::string &var_name, const Value &value) override
    {
        manager_[var_name] = value;
    }

    virtual bool Remove(const std::string &var_name) override
    {
        if (Contains(var_name))
        {
            manager_.erase(var_name);
            return true;
        }
        return false;
    }

    virtual void Clear() override
    {
        manager_.clear();
    }

    virtual bool Contains(const std::string &var_name) const override
    {
        return manager_.find(var_name) != manager_.end();
    }

    virtual std::unordered_set<std::string> keys() const override
    {
        std::unordered_set<std::string> key_set;
        for (const auto &entry : manager_)
        {
            key_set.insert(entry.first);
        }
        return key_set;
    }

  private:
    std::unordered_map<std::string, Value> manager_;
};

class VariableManagerTest : public testing::Test
{
  protected:
    VariableManagerTest(): manager_(std::unique_ptr<MockExprContext>(new MockExprContext()), EvalExpr, ParseExpr){}

    void SetUp() override
    {
        manager_.Reset();
    }

    void VerifyVar(const Variable *var, Variable::Type type, VariableStatus status, Value value)
    {
        EXPECT_TRUE(var != nullptr);
        EXPECT_TRUE(var->GetType() == type);
        EXPECT_TRUE(var->status() == status);
        if (type == Variable::Type::Expr)
        {
            EXPECT_TRUE(var->As<ExprVariable>()->expression() == value.Cast<std::string>());
        }
        else if (type == Variable::Type::Raw)
        {
            EXPECT_TRUE(var->As<RawVariable>()->value() == value);
        }
    }

    void VerifyNode(
        const DependencyGraph::Node *node, const std::vector<std::string> &dependencies,
        const std::vector<std::string> &dependents
    )
    {
        EXPECT_TRUE(node != nullptr);
        EXPECT_EQ(node->dependencies().size(), dependencies.size());
        for (const auto &dep : dependencies)
        {
            EXPECT_NE(node->dependencies().find(dep), node->dependencies().end());
        }
        EXPECT_EQ(node->dependents().size(), dependents.size());
        for (const auto &dep : dependents)
        {
            EXPECT_NE(node->dependents().find(dep), node->dependents().end());
        }
    }

    void VerifyEdges(const std::vector<DependencyGraph::Edge> &edge_list, bool is_all = false)
    {
        if (is_all)
        {
            auto range = manager_.graph()->GetAllEdges();
            EXPECT_TRUE(std::distance(range.first, range.second) == edge_list.size());
        }
        for (const auto &edge : edge_list)
        {
            EXPECT_TRUE(manager_.graph()->IsEdgeExist(edge));
        }
    }

    VariableManager manager_;
};

TEST_F(VariableManagerTest, AddAndRemoveVariable)
{
    // test AddVariable
    std::unique_ptr<Variable> raw_var = VariableFactory::CreateRawVariable("A", 1);
    EXPECT_TRUE(manager_.AddVariable(std::move(raw_var)));
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kRawVar, 1);
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
    EXPECT_FALSE(manager_.AddVariable(VariableFactory::CreateRawVariable("A", 2)));

    // test AddVariables
    std::vector<std::unique_ptr<Variable>> var_vec;
    var_vec.push_back(VariableFactory::CreateRawVariable("B", 2));
    var_vec.push_back(VariableFactory::CreateExprVariable("C", "A + B"));
    EXPECT_TRUE(manager_.AddVariables(std::move(var_vec)));
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(manager_.graph()->GetNode("C"), {"A", "B"}, {});
    VerifyEdges({{"C", "A"}, {"C", "B"}}, true);
    var_vec.clear();
    var_vec.push_back(VariableFactory::CreateRawVariable("C", 4));
    var_vec.push_back(VariableFactory::CreateExprVariable("D", "C"));
    EXPECT_FALSE(manager_.AddVariables(std::move(var_vec)));
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(manager_.graph()->GetNode("C"), {"A", "B"}, {"D"});
    VerifyNode(manager_.graph()->GetNode("D"), {"C"}, {});
    VerifyEdges({{"D", "C"}});

    // test RemoveVariable
    EXPECT_TRUE(manager_.RemoveVariable("A"));
    EXPECT_FALSE(manager_.IsVariableExist("A"));
    VerifyNode(manager_.graph()->GetNode("C"), {"B"}, {"D"});
    VerifyEdges({{"C", "A"}});
    EXPECT_FALSE(manager_.RemoveVariable("A"));

    // test RemoveVariables
    EXPECT_TRUE(manager_.RemoveVariables({"B", "C"}));
    EXPECT_FALSE(manager_.IsVariableExist("B"));
    EXPECT_FALSE(manager_.IsVariableExist("C"));
    VerifyNode(manager_.graph()->GetNode("D"), {}, {});
    VerifyEdges({{"D", "C"}}, true);
    EXPECT_FALSE(manager_.RemoveVariables({"B", "D"}));
    EXPECT_FALSE(manager_.IsVariableExist("D"));
    VerifyEdges({}, true);
}

TEST_F(VariableManagerTest, SetVariable)
{
    // test SetVariable
    EXPECT_FALSE(manager_.SetVariable("B", VariableFactory::CreateRawVariable("A", 1)));
    EXPECT_FALSE(manager_.IsVariableExist("B"));
    EXPECT_TRUE(manager_.SetVariable("A", VariableFactory::CreateRawVariable("A", 1)));
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kRawVar, 1);
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});

    // test SetValue
    manager_.SetValue("A", 3);
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kRawVar, 3);
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
    manager_.SetValue("B", 5);
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyNode(manager_.graph()->GetNode("B"), {}, {});

    // test SetExpression
    manager_.SetExpression("C", "A +");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSyntaxError, "A +");
    VerifyNode(manager_.graph()->GetNode("C"), {}, {});
    VerifyEdges({}, true);
    manager_.SetExpression("C", "A + B");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(manager_.graph()->GetNode("C"), {"A", "B"}, {});
    VerifyEdges({{"C", "A"}, {"C", "B"}}, true);
    EXPECT_THROW(manager_.SetExpression("B", "C"), DependencyCycleException);
}

TEST_F(VariableManagerTest, CycleDetection)
{
    // before detection cycle
    manager_.SetExpression("A", "B*C");
    manager_.SetExpression("B", "D");
    manager_.SetValue("C", 2);
    VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(manager_.graph()->GetNode("B"), {}, {"A"});
    VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B*C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Raw, VariableStatus::kRawVar, 2);
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    // AddVariables
    std::vector<std::unique_ptr<Variable>> var_vec;
    var_vec.push_back(VariableFactory::CreateRawVariable("E", 4));
    var_vec.push_back(VariableFactory::CreateExprVariable("D", "A"));
    EXPECT_THROW(manager_.AddVariables(std::move(var_vec)), DependencyCycleException);
    VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(manager_.graph()->GetNode("B"), {}, {"A"});
    VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B*C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Raw, VariableStatus::kRawVar, 2);
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    manager_.SetExpression("D", "E");
    EXPECT_THROW(manager_.SetExpression("E", "B"), DependencyCycleException);
    VerifyNode(manager_.graph()->GetNode("D"), {}, {"B"});
    VerifyVar(manager_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kParseSuccess, "E");
    EXPECT_FALSE(manager_.IsVariableExist("E"));
}

TEST_F(VariableManagerTest, RenameVariable)
{
    manager_.SetExpression("A", "B+C");
    manager_.SetExpression("B", "D+E");
    manager_.SetExpression("C", "F");
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}, {"B", "E"}, {"C", "F"}}, true);
    manager_.SetValue("D", 1);
    manager_.SetValue("E", 5);
    VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(manager_.graph()->GetNode("B"), {"D", "E"}, {"A"});
    VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
    manager_.RenameVariable("B", "F");
    VerifyNode(manager_.graph()->GetNode("A"), {"C"}, {});
    VerifyNode(manager_.graph()->GetNode("C"), {"F"}, {"A"});
    VerifyNode(manager_.graph()->GetNode("F"), {"D", "E"}, {"C"});
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"F", "D"}, {"F", "E"}, {"C", "F"}}, true);
}

TEST_F(VariableManagerTest, UpdateContext)
{
    manager_.SetExpression("A", "B+C");
    manager_.SetExpression("B", "D+E");
    manager_.SetExpression("C", "F");
    manager_.SetValue("D", 1);
    manager_.SetValue("E", 5);
    manager_.SetValue("F", 10);
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B+C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D+E");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "F");
    VerifyVar(manager_.GetVariable("D"), Variable::Type::Raw, VariableStatus::kRawVar, 1);
    VerifyVar(manager_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(manager_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    manager_.Update();
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "B+C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "F");
    VerifyVar(manager_.GetVariable("D"), Variable::Type::Raw, VariableStatus::kRawVar, 1);
    VerifyVar(manager_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(manager_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_TRUE(manager_.context()->Contains("A"));
    EXPECT_TRUE(manager_.context()->Contains("B"));
    EXPECT_TRUE(manager_.context()->Contains("C"));
    EXPECT_TRUE(manager_.context()->Contains("D"));
    EXPECT_TRUE(manager_.context()->Contains("E"));
    EXPECT_TRUE(manager_.context()->Contains("F"));
    EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 1);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

    manager_.RemoveVariable("D");
    EXPECT_TRUE(manager_.context()->Contains("D"));
    EXPECT_FALSE(manager_.IsVariableExist("D"));
    manager_.Update();
    EXPECT_FALSE(manager_.context()->Contains("D"));
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalNameError, "B+C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kMissingDependency, "D+E");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "F");
    VerifyVar(manager_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(manager_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_FALSE(manager_.context()->Contains("A"));
    EXPECT_FALSE(manager_.context()->Contains("B"));
    EXPECT_TRUE(manager_.context()->Contains("C"));
    EXPECT_FALSE(manager_.context()->Contains("D"));
    EXPECT_TRUE(manager_.context()->Contains("E"));
    EXPECT_TRUE(manager_.context()->Contains("F"));
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

    manager_.SetExpression("D", "E");
    manager_.SetExpression("C", "E+F");
    manager_.Update();
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "B+C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E+F");
    VerifyVar(manager_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E");
    VerifyVar(manager_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(manager_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_TRUE(manager_.context()->Contains("A"));
    EXPECT_TRUE(manager_.context()->Contains("B"));
    EXPECT_TRUE(manager_.context()->Contains("C"));
    EXPECT_TRUE(manager_.context()->Contains("D"));
    EXPECT_TRUE(manager_.context()->Contains("E"));
    EXPECT_TRUE(manager_.context()->Contains("F"));
    EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 25);
    EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 15);
    EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

    manager_.SetValue("E", 6);
    manager_.UpdateVariable("E");
    EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 28);
    EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 12);
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

    manager_.SetValue("F", 0);
    manager_.SetExpression("C", "5/F");
    manager_.Update();
    VerifyVar(manager_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalNameError, "B+C");
    VerifyVar(manager_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(manager_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalZeroDivisionError, "5/F");
    VerifyVar(manager_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E");
    VerifyVar(manager_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 6);
    VerifyVar(manager_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 0);
    EXPECT_FALSE(manager_.context()->Contains("A"));
    EXPECT_FALSE(manager_.context()->Contains("C"));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}