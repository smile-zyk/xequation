#include "core/dependency_graph.h"
#include "core/expr_common.h"
#include "core/expr_context.h"
#include "core/equation.h"
#include "core/equation_manager.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <regex>
#include <string>

#include <gtest/gtest.h>
#include <unordered_set>

using namespace xequation;

ParseResult ParseExpr(const std::string &code)
{
    ParseResult result;
    std::string name = code.substr(0,1);
    std::string expr = code.substr(2);
    result.name = name;
    result.content = code;
    result.type = ParseType::kVarDecl;
    std::regex expr_regex(R"(^\s*(([A-Z]+|\d+)(\s*([\+\-\*\/])\s*([A-Z]+|\d+))?)\s*$)");
    std::smatch match;
    if (std::regex_match(expr, match, expr_regex))
    {
        // extract Equations and operator
        if (std::regex_match(match[2].str(), std::regex(R"(^[A-Z]+$)")))
        {
            result.dependencies.push_back(match[2]);
        }
        if (std::regex_match(match[5].str(), std::regex(R"(^[A-Z]+$)")))
        {
            result.dependencies.push_back(match[5]);
        }
    }
    else
    {
        throw ParseException("parse error" + code);
    }
    return result;
}

ExecResult ExecExpr(const std::string &code, ExprContext *context)
{
    ExecResult result;
    std::regex expr_regex(R"(^\s*(([A-Z]+|\d+)(\s*([\+\-\*\/])\s*([A-Z]+|\d+))?)\s*$)");
    std::smatch match;

    std::string name = code.substr(0,1);
    std::string expr = code.substr(2);

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
                result.status = ExecStatus::kTypeError;
                result.message = "Equation " + var1 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = ExecStatus::kNameError;
            result.message = "Equation " + var1 + " not found";
            return result;
        }

        if (!match[4].matched)
        {
            result.status = ExecStatus::kSuccess;
            context->Set(name, val1);
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
                result.status = ExecStatus::kTypeError;
                result.message = "Equation " + var2 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = ExecStatus::kNameError;
            result.message = "Equation " + var2 + " not found";
            return result;
        }

        if (op == "+")
        {
            result.status = ExecStatus::kSuccess;
            context->Set(name, val1 + val2);
        }
        else if (op == "-")
        {
            result.status = ExecStatus::kSuccess;
            context->Set(name, val1 - val2);
        }
        else if (op == "*")
        {
            result.status = ExecStatus::kSuccess;
            context->Set(name, val1 * val2);
        }
        else if (op == "/")
        {
            if (val2 == 0)
            {
                result.status = ExecStatus::kZeroDivisionError;
            }
            else
            {
                result.status = ExecStatus::kSuccess;
                context->Set(name, val1 / val2);
            }
        }
        else
        {
            result.status = ExecStatus::kAttributeError;
            result.message = "Invalid operator: " + op;
        }
    }
    else
    {
        result.status = ExecStatus::kSyntaxError;
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

class EquationManagerTest : public testing::Test
{
  protected:
    EquationManagerTest(): manager_(std::unique_ptr<MockExprContext>(new MockExprContext()), ExecExpr, ParseExpr){}

    void SetUp() override
    {
        manager_.Reset();
    }

    void VerifyVar(const Equation *var, ParseType type, ExecStatus status, const std::string& name)
    {
        EXPECT_TRUE(var != nullptr);
        EXPECT_TRUE(var->type() == type);
        EXPECT_TRUE(var->status() == status);
        EXPECT_TRUE(var->name() == name);
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

    EquationManager manager_;
};

TEST_F(EquationManagerTest, AddAndRemoveEquation)
{
    manager_.AddEquation("A=1");
    VerifyVar(manager_.GetEquation("A"), ParseType::kVarDecl, ExecStatus::kInit, "A");
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
    EXPECT_THROW(manager_.AddEquation("A=2"), DuplicateEquationNameError);

    manager_.AddEquation("B=A+C");
    VerifyVar(manager_.GetEquation("B"), ParseType::kVarDecl, ExecStatus::kInit, "B");
    VerifyNode(manager_.graph()->GetNode("B"), {"A"}, {});
    VerifyNode(manager_.graph()->GetNode("A"), {}, {"B"});
    VerifyEdges({{"B", "A"}, {"B", "C"}}, true);

    manager_.RemoveEquation("B");
    EXPECT_EQ(manager_.GetEquation("B"), nullptr);
    VerifyEdges({}, true);
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
}

// TEST_F(EquationManagerTest, SetEquation)
// {
//     // test SetEquation
//     EXPECT_FALSE(manager_.SetEquation("B", EquationFactory::CreateRawEquation("A", 1)));
//     EXPECT_FALSE(manager_.IsEquationExist("B"));
//     EXPECT_TRUE(manager_.SetEquation("A", EquationFactory::CreateRawEquation("A", 1)));
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Raw, EquationStatus::kRawVar, 1);
//     VerifyNode(manager_.graph()->GetNode("A"), {}, {});

//     // test SetValue
//     manager_.SetValue("A", 3);
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Raw, EquationStatus::kRawVar, 3);
//     VerifyNode(manager_.graph()->GetNode("A"), {}, {});
//     manager_.SetValue("B", 5);
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Raw, EquationStatus::kRawVar, 5);
//     VerifyNode(manager_.graph()->GetNode("B"), {}, {});

//     // test SetExpression
//     manager_.SetExpression("C", "A +");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kParseSyntaxError, "A +");
//     VerifyNode(manager_.graph()->GetNode("C"), {}, {});
//     VerifyEdges({}, true);
//     manager_.SetExpression("C", "A + B");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kParseSuccess, "A + B");
//     VerifyNode(manager_.graph()->GetNode("C"), {"A", "B"}, {});
//     VerifyEdges({{"C", "A"}, {"C", "B"}}, true);
//     EXPECT_THROW(manager_.SetExpression("B", "C"), DependencyCycleException);
// }

// TEST_F(EquationManagerTest, CycleDetection)
// {
//     // before detection cycle
//     manager_.SetExpression("A", "B*C");
//     manager_.SetExpression("B", "D");
//     manager_.SetValue("C", 2);
//     VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
//     VerifyNode(manager_.graph()->GetNode("B"), {}, {"A"});
//     VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kParseSuccess, "B*C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kParseSuccess, "D");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Raw, EquationStatus::kRawVar, 2);
//     VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

//     // AddEquations
//     std::vector<std::unique_ptr<Equation>> var_vec;
//     var_vec.push_back(EquationFactory::CreateRawEquation("E", 4));
//     var_vec.push_back(EquationFactory::CreateExprEquation("D", "A"));
//     EXPECT_THROW(manager_.AddEquations(std::move(var_vec)), DependencyCycleException);
//     VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
//     VerifyNode(manager_.graph()->GetNode("B"), {}, {"A"});
//     VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kParseSuccess, "B*C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kParseSuccess, "D");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Raw, EquationStatus::kRawVar, 2);
//     VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

//     manager_.SetExpression("D", "E");
//     EXPECT_THROW(manager_.SetExpression("E", "B"), DependencyCycleException);
//     VerifyNode(manager_.graph()->GetNode("D"), {}, {"B"});
//     VerifyVar(manager_.GetEquation("D"), Equation::Type::Expr, EquationStatus::kParseSuccess, "E");
//     EXPECT_FALSE(manager_.IsEquationExist("E"));
// }

// TEST_F(EquationManagerTest, RenameEquation)
// {
//     manager_.SetExpression("A", "B+C");
//     manager_.SetExpression("B", "D+E");
//     manager_.SetExpression("C", "F");
//     VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}, {"B", "E"}, {"C", "F"}}, true);
//     manager_.SetValue("D", 1);
//     manager_.SetValue("E", 5);
//     VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
//     VerifyNode(manager_.graph()->GetNode("B"), {"D", "E"}, {"A"});
//     VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
//     manager_.RenameEquation("B", "F");
//     VerifyNode(manager_.graph()->GetNode("A"), {"C"}, {});
//     VerifyNode(manager_.graph()->GetNode("C"), {"F"}, {"A"});
//     VerifyNode(manager_.graph()->GetNode("F"), {"D", "E"}, {"C"});
//     VerifyEdges({{"A", "B"}, {"A", "C"}, {"F", "D"}, {"F", "E"}, {"C", "F"}}, true);
// }

// TEST_F(EquationManagerTest, UpdateContext)
// {
//     manager_.SetExpression("A", "B+C");
//     manager_.SetExpression("B", "D+E");
//     manager_.SetExpression("C", "F");
//     manager_.SetValue("D", 1);
//     manager_.SetValue("E", 5);
//     manager_.SetValue("F", 10);
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kParseSuccess, "B+C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kParseSuccess, "D+E");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kParseSuccess, "F");
//     VerifyVar(manager_.GetEquation("D"), Equation::Type::Raw, EquationStatus::kRawVar, 1);
//     VerifyVar(manager_.GetEquation("E"), Equation::Type::Raw, EquationStatus::kRawVar, 5);
//     VerifyVar(manager_.GetEquation("F"), Equation::Type::Raw, EquationStatus::kRawVar, 10);
//     manager_.Update();
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "B+C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "D+E");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "F");
//     VerifyVar(manager_.GetEquation("D"), Equation::Type::Raw, EquationStatus::kRawVar, 1);
//     VerifyVar(manager_.GetEquation("E"), Equation::Type::Raw, EquationStatus::kRawVar, 5);
//     VerifyVar(manager_.GetEquation("F"), Equation::Type::Raw, EquationStatus::kRawVar, 10);
//     EXPECT_TRUE(manager_.context()->Contains("A"));
//     EXPECT_TRUE(manager_.context()->Contains("B"));
//     EXPECT_TRUE(manager_.context()->Contains("C"));
//     EXPECT_TRUE(manager_.context()->Contains("D"));
//     EXPECT_TRUE(manager_.context()->Contains("E"));
//     EXPECT_TRUE(manager_.context()->Contains("F"));
//     EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 16);
//     EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 6);
//     EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 10);
//     EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 1);
//     EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
//     EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

//     manager_.RemoveEquation("D");
//     EXPECT_TRUE(manager_.context()->Contains("D"));
//     EXPECT_FALSE(manager_.IsEquationExist("D"));
//     manager_.Update();
//     EXPECT_FALSE(manager_.context()->Contains("D"));
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kExprEvalNameError, "B+C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kMissingDependency, "D+E");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "F");
//     VerifyVar(manager_.GetEquation("E"), Equation::Type::Raw, EquationStatus::kRawVar, 5);
//     VerifyVar(manager_.GetEquation("F"), Equation::Type::Raw, EquationStatus::kRawVar, 10);
//     EXPECT_FALSE(manager_.context()->Contains("A"));
//     EXPECT_FALSE(manager_.context()->Contains("B"));
//     EXPECT_TRUE(manager_.context()->Contains("C"));
//     EXPECT_FALSE(manager_.context()->Contains("D"));
//     EXPECT_TRUE(manager_.context()->Contains("E"));
//     EXPECT_TRUE(manager_.context()->Contains("F"));
//     EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 10);
//     EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
//     EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

//     manager_.SetExpression("D", "E");
//     manager_.SetExpression("C", "E+F");
//     manager_.Update();
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "B+C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "D+E");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "E+F");
//     VerifyVar(manager_.GetEquation("D"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "E");
//     VerifyVar(manager_.GetEquation("E"), Equation::Type::Raw, EquationStatus::kRawVar, 5);
//     VerifyVar(manager_.GetEquation("F"), Equation::Type::Raw, EquationStatus::kRawVar, 10);
//     EXPECT_TRUE(manager_.context()->Contains("A"));
//     EXPECT_TRUE(manager_.context()->Contains("B"));
//     EXPECT_TRUE(manager_.context()->Contains("C"));
//     EXPECT_TRUE(manager_.context()->Contains("D"));
//     EXPECT_TRUE(manager_.context()->Contains("E"));
//     EXPECT_TRUE(manager_.context()->Contains("F"));
//     EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 25);
//     EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 10);
//     EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 15);
//     EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 5);
//     EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
//     EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

//     manager_.SetValue("E", 6);
//     manager_.UpdateEquation("E");
//     EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 28);
//     EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 12);
//     EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 16);
//     EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 6);
//     EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 6);
//     EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

//     manager_.SetValue("F", 0);
//     manager_.SetExpression("C", "5/F");
//     manager_.Update();
//     VerifyVar(manager_.GetEquation("A"), Equation::Type::Expr, EquationStatus::kExprEvalNameError, "B+C");
//     VerifyVar(manager_.GetEquation("B"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "D+E");
//     VerifyVar(manager_.GetEquation("C"), Equation::Type::Expr, EquationStatus::kExprEvalZeroDivisionError, "5/F");
//     VerifyVar(manager_.GetEquation("D"), Equation::Type::Expr, EquationStatus::kExprEvalSuccess, "E");
//     VerifyVar(manager_.GetEquation("E"), Equation::Type::Raw, EquationStatus::kRawVar, 6);
//     VerifyVar(manager_.GetEquation("F"), Equation::Type::Raw, EquationStatus::kRawVar, 0);
//     EXPECT_FALSE(manager_.context()->Contains("A"));
//     EXPECT_FALSE(manager_.context()->Contains("C"));
// }

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}