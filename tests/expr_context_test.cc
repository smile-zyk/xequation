#include "dependency_graph.h"
#include "expr_common.h"
#include "expr_context.h"
#include "variable.h"

#include <iterator>
#include <memory>
#include <regex>
#include <string>

#include <gtest/gtest.h>
#include <unordered_set>

using namespace xexprengine;

//
class MockExprContext : public ExprContext
{
  public:
    MockExprContext() : ExprContext()
    {
        set_parse_callback([this](const std::string &expr) { return this->ParseCallback(expr); });
        set_evaluate_callback([this](const std::string &expr) {
            return this->EvaluateCallback(expr, this);
        });
    }

    virtual Value GetContextValue(const std::string &var_name) const override
    {
        if (IsContextValueExist(var_name))
        {
            return context_.at(var_name);
        }
        return Value::Null();
    }

    virtual void SetContextValue(const std::string &var_name, const Value &value) override
    {
        context_[var_name] = value;
    }

    virtual bool RemoveContextValue(const std::string &var_name) override
    {
        if (IsContextValueExist(var_name))
        {
            context_.erase(var_name);
            return true;
        }
        return false;
    }

    virtual void ClearContextValue() override
    {
        context_.clear();
    }

    virtual bool IsContextValueExist(const std::string &var_name) const override
    {
        return context_.find(var_name) != context_.end();
    }

    virtual std::unordered_set<std::string> GetContextExistVariables() const override
    {
        std::unordered_set<std::string> key_set;
        for (const auto &entry : context_)
        {
            key_set.insert(entry.first);
        }
        return key_set;
    }

    ParseResult ParseCallback(const std::string &expr)
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

    EvalResult EvaluateCallback(const std::string &expr, const ExprContext *context)
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
            else if (context->IsContextValueExist(var1))
            {
                Value v1 = context->GetContextValue(var1);
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
            else if (context->IsContextValueExist(var2))
            {
                Value v2 = context->GetContextValue(var2);
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

  private:
    std::unordered_map<std::string, Value> context_;
};

class ExprContextTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        context_.Reset();
    }

    void VerifyVar(Variable *var, Variable::Type type, VariableStatus status, Value value)
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
            auto range = context_.graph()->GetAllEdges();
            EXPECT_TRUE(std::distance(range.first, range.second) == edge_list.size());
        }
        for (const auto &edge : edge_list)
        {
            EXPECT_TRUE(context_.graph()->IsEdgeExist(edge));
        }
    }

    MockExprContext context_;
};

TEST_F(ExprContextTest, AddAndRemoveVariable)
{
    // test AddVariable
    std::unique_ptr<Variable> raw_var = VariableFactory::CreateRawVariable("A", 1);
    EXPECT_TRUE(context_.AddVariable(std::move(raw_var)));
    VerifyVar(context_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kParseSuccess, 1);
    VerifyNode(context_.graph()->GetNode("A"), {}, {});
    EXPECT_FALSE(context_.AddVariable(VariableFactory::CreateRawVariable("A", 2)));

    // test AddVariables
    std::vector<std::unique_ptr<Variable>> var_vec;
    var_vec.push_back(VariableFactory::CreateRawVariable("B", 2));
    var_vec.push_back(VariableFactory::CreateExprVariable("C", "A + B"));
    EXPECT_TRUE(context_.AddVariables(std::move(var_vec)));
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(context_.graph()->GetNode("C"), {"A", "B"}, {});
    VerifyEdges({{"C", "A"}, {"C", "B"}}, true);
    var_vec.clear();
    var_vec.push_back(VariableFactory::CreateRawVariable("C", 4));
    var_vec.push_back(VariableFactory::CreateExprVariable("D", "C"));
    EXPECT_FALSE(context_.AddVariables(std::move(var_vec)));
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(context_.graph()->GetNode("C"), {"A", "B"}, {"D"});
    VerifyNode(context_.graph()->GetNode("D"), {"C"}, {});
    VerifyEdges({{"D", "C"}});

    // test RemoveVariable
    EXPECT_TRUE(context_.RemoveVariable("A"));
    EXPECT_FALSE(context_.IsVariableExist("A"));
    VerifyNode(context_.graph()->GetNode("C"), {"B"}, {"D"});
    VerifyEdges({{"C", "A"}});
    EXPECT_FALSE(context_.RemoveVariable("A"));

    // test RemoveVariables
    EXPECT_TRUE(context_.RemoveVariables({"B", "C"}));
    EXPECT_FALSE(context_.IsVariableExist("B"));
    EXPECT_FALSE(context_.IsVariableExist("C"));
    VerifyNode(context_.graph()->GetNode("D"), {}, {});
    VerifyEdges({{"D", "C"}}, true);
    EXPECT_FALSE(context_.RemoveVariables({"B", "D"}));
    EXPECT_FALSE(context_.IsVariableExist("D"));
    VerifyEdges({}, true);
}

TEST_F(ExprContextTest, SetVariable)
{
    // test SetVariable
    EXPECT_FALSE(context_.SetVariable("B", VariableFactory::CreateRawVariable("A", 1)));
    EXPECT_FALSE(context_.IsVariableExist("B"));
    EXPECT_TRUE(context_.SetVariable("A", VariableFactory::CreateRawVariable("A", 1)));
    VerifyVar(context_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kParseSuccess, 1);
    VerifyNode(context_.graph()->GetNode("A"), {}, {});

    // test SetValue
    context_.SetValue("A", 3);
    VerifyVar(context_.GetVariable("A"), Variable::Type::Raw, VariableStatus::kParseSuccess, 3);
    VerifyNode(context_.graph()->GetNode("A"), {}, {});
    context_.SetValue("B", 5);
    VerifyVar(context_.GetVariable("B"), Variable::Type::Raw, VariableStatus::kParseSuccess, 5);
    VerifyNode(context_.graph()->GetNode("B"), {}, {});

    // test SetExpression
    context_.SetExpression("C", "A +");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSyntaxError, "A +");
    VerifyNode(context_.graph()->GetNode("C"), {}, {});
    VerifyEdges({}, true);
    context_.SetExpression("C", "A + B");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "A + B");
    VerifyNode(context_.graph()->GetNode("C"), {"A", "B"}, {});
    VerifyEdges({{"C", "A"}, {"C", "B"}}, true);
    EXPECT_THROW(context_.SetExpression("B", "C"), DependencyCycleException);
}

TEST_F(ExprContextTest, CycleDetection)
{
    // before detection cycle
    context_.SetExpression("A", "B*C");
    context_.SetExpression("B", "D");
    context_.SetValue("C", 2);
    VerifyNode(context_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(context_.graph()->GetNode("B"), {}, {"A"});
    VerifyNode(context_.graph()->GetNode("C"), {}, {"A"});
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B*C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Raw, VariableStatus::kParseSuccess, 2);
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    // AddVariables
    std::vector<std::unique_ptr<Variable>> var_vec;
    var_vec.push_back(VariableFactory::CreateRawVariable("E", 4));
    var_vec.push_back(VariableFactory::CreateExprVariable("D", "A"));
    EXPECT_THROW(context_.AddVariables(std::move(var_vec)), DependencyCycleException);
    VerifyNode(context_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(context_.graph()->GetNode("B"), {}, {"A"});
    VerifyNode(context_.graph()->GetNode("C"), {}, {"A"});
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B*C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Raw, VariableStatus::kParseSuccess, 2);
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    context_.SetExpression("D", "E");
    EXPECT_THROW(context_.SetExpression("E", "B"), DependencyCycleException);
    VerifyNode(context_.graph()->GetNode("D"), {}, {"B"});
    VerifyVar(context_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kParseSuccess, "E");
    EXPECT_FALSE(context_.IsVariableExist("E"));
}

TEST_F(ExprContextTest, RenameVariable)
{
    context_.SetExpression("A", "B+C");
    context_.SetExpression("B", "D+E");
    context_.SetExpression("C", "F");
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}, {"B", "E"}, {"C", "F"}}, true);
    context_.SetValue("D", 1);
    context_.SetValue("E", 5);
    VerifyNode(context_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(context_.graph()->GetNode("B"), {"D", "E"}, {"A"});
    VerifyNode(context_.graph()->GetNode("C"), {}, {"A"});
    context_.RenameVariable("B", "F");
    VerifyNode(context_.graph()->GetNode("A"), {"C"}, {});
    VerifyNode(context_.graph()->GetNode("C"), {"F"}, {"A"});
    VerifyNode(context_.graph()->GetNode("F"), {"D", "E"}, {"C"});
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"F", "D"}, {"F", "E"}, {"C", "F"}}, true);
}

TEST_F(ExprContextTest, UpdateContext)
{
    context_.SetExpression("A", "B+C");
    context_.SetExpression("B", "D+E");
    context_.SetExpression("C", "F");
    context_.SetValue("D", 1);
    context_.SetValue("E", 5);
    context_.SetValue("F", 10);
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kParseSuccess, "B+C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kParseSuccess, "D+E");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kParseSuccess, "F");
    VerifyVar(context_.GetVariable("D"), Variable::Type::Raw, VariableStatus::kParseSuccess, 1);
    VerifyVar(context_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kParseSuccess, 5);
    VerifyVar(context_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kParseSuccess, 10);
    context_.Update();
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "B+C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "F");
    VerifyVar(context_.GetVariable("D"), Variable::Type::Raw, VariableStatus::kRawVar, 1);
    VerifyVar(context_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(context_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_TRUE(context_.IsContextValueExist("A"));
    EXPECT_TRUE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.IsContextValueExist("C"));
    EXPECT_TRUE(context_.IsContextValueExist("D"));
    EXPECT_TRUE(context_.IsContextValueExist("E"));
    EXPECT_TRUE(context_.IsContextValueExist("F"));
    EXPECT_TRUE(context_.GetContextValue("A").Cast<int>() == 16);
    EXPECT_TRUE(context_.GetContextValue("B").Cast<int>() == 6);
    EXPECT_TRUE(context_.GetContextValue("C").Cast<int>() == 10);
    EXPECT_TRUE(context_.GetContextValue("D").Cast<int>() == 1);
    EXPECT_TRUE(context_.GetContextValue("E").Cast<int>() == 5);
    EXPECT_TRUE(context_.GetContextValue("F").Cast<int>() == 10);

    context_.RemoveVariable("D");
    EXPECT_TRUE(context_.IsContextValueExist("D"));
    EXPECT_FALSE(context_.IsVariableExist("D"));
    context_.Update();
    EXPECT_FALSE(context_.IsContextValueExist("D"));
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalNameError, "B+C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kMissingDependency, "D+E");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "F");
    VerifyVar(context_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(context_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_FALSE(context_.IsContextValueExist("A"));
    EXPECT_FALSE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.IsContextValueExist("C"));
    EXPECT_FALSE(context_.IsContextValueExist("D"));
    EXPECT_TRUE(context_.IsContextValueExist("E"));
    EXPECT_TRUE(context_.IsContextValueExist("F"));
    EXPECT_TRUE(context_.GetContextValue("C").Cast<int>() == 10);
    EXPECT_TRUE(context_.GetContextValue("E").Cast<int>() == 5);
    EXPECT_TRUE(context_.GetContextValue("F").Cast<int>() == 10);

    context_.SetExpression("D", "E");
    context_.SetExpression("C", "E+F");
    context_.Update();
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "B+C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E+F");
    VerifyVar(context_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E");
    VerifyVar(context_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 5);
    VerifyVar(context_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 10);
    EXPECT_TRUE(context_.IsContextValueExist("A"));
    EXPECT_TRUE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.IsContextValueExist("C"));
    EXPECT_TRUE(context_.IsContextValueExist("D"));
    EXPECT_TRUE(context_.IsContextValueExist("E"));
    EXPECT_TRUE(context_.IsContextValueExist("F"));
    EXPECT_TRUE(context_.GetContextValue("A").Cast<int>() == 25);
    EXPECT_TRUE(context_.GetContextValue("B").Cast<int>() == 10);
    EXPECT_TRUE(context_.GetContextValue("C").Cast<int>() == 15);
    EXPECT_TRUE(context_.GetContextValue("D").Cast<int>() == 5);
    EXPECT_TRUE(context_.GetContextValue("E").Cast<int>() == 5);
    EXPECT_TRUE(context_.GetContextValue("F").Cast<int>() == 10);

    context_.SetValue("E", 6);
    context_.Update();
    EXPECT_TRUE(context_.GetContextValue("A").Cast<int>() == 28);
    EXPECT_TRUE(context_.GetContextValue("B").Cast<int>() == 12);
    EXPECT_TRUE(context_.GetContextValue("C").Cast<int>() == 16);
    EXPECT_TRUE(context_.GetContextValue("D").Cast<int>() == 6);
    EXPECT_TRUE(context_.GetContextValue("E").Cast<int>() == 6);
    EXPECT_TRUE(context_.GetContextValue("F").Cast<int>() == 10);

    context_.SetValue("F", 0);
    context_.SetExpression("C", "5/F");
    context_.Update();
    VerifyVar(context_.GetVariable("A"), Variable::Type::Expr, VariableStatus::kExprEvalNameError, "B+C");
    VerifyVar(context_.GetVariable("B"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "D+E");
    VerifyVar(context_.GetVariable("C"), Variable::Type::Expr, VariableStatus::kExprEvalZeroDivisionError, "5/F");
    VerifyVar(context_.GetVariable("D"), Variable::Type::Expr, VariableStatus::kExprEvalSuccess, "E");
    VerifyVar(context_.GetVariable("E"), Variable::Type::Raw, VariableStatus::kRawVar, 6);
    VerifyVar(context_.GetVariable("F"), Variable::Type::Raw, VariableStatus::kRawVar, 0);
    EXPECT_FALSE(context_.IsContextValueExist("A"));
    EXPECT_FALSE(context_.IsContextValueExist("C"));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}