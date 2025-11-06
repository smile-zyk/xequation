#include "core/dependency_graph.h"
#include "core/equation.h"
#include "core/equation_manager.h"
#include "core/equation_common.h"
#include "core/equation_context.h"

#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <unordered_set>

using namespace xequation;

class EquationParser
{
  public:
    static std::vector<Equation> parseMultipleExpressions(const std::string &input)
    {
        std::vector<Equation> equations;

        size_t start = 0;
        size_t end = 0;

        while (end != std::string::npos)
        {
            end = input.find(';', start);

            std::string expr = input.substr(start, (end == std::string::npos) ? std::string::npos : end - start);

            expr = std::regex_replace(expr, std::regex(R"(^\s+|\s+$)"), "");

            if (!expr.empty())
            {
                Equation equation = parseExpression(expr);
                equations.push_back(equation);
            }

            if (end != std::string::npos)
            {
                start = end + 1;
            }
        }

        return equations;
    }

  private:
    static Equation parseExpression(const std::string &expr)
    {
        Equation equation;
        equation.set_content(expr);

        std::regex assign_regex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)\s*$)");
        std::smatch assign_match;

        if (std::regex_match(expr, assign_match, assign_regex))
        {
            std::string variable_name = assign_match[1].str();
            std::string expression = assign_match[2].str();

            equation.set_name(variable_name);

            parseDependencies(expression, equation);
            equation.set_type(Equation::Type::kVariable);
        }
        else
        {
            throw ParseException("Syntax error: assignment operator '=' not found or variable name missing");
        }

        return equation;
    }

    static void parseDependencies(const std::string &expr, Equation &equation)
    {
        std::regex var_regex(R"(\b[A-Za-z_][A-Za-z0-9_]*\b)");
        auto words_begin = std::sregex_iterator(expr.begin(), expr.end(), var_regex);
        auto words_end = std::sregex_iterator();

        std::vector<std::string> res;

        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
            std::string var_name = i->str();

            if (std::regex_match(var_name, std::regex(R"(^\d+$)")))
            {
                continue;
            }

            res.push_back(var_name);
        }
        std::sort(res.begin(), res.end());
        auto last = std::unique(res.begin(), res.end());
        res.erase(last, res.end());
        equation.set_dependencies(res);
    }
};

ExecResult ExecExpr(const std::string &code, EquationContext *context)
{
    ExecResult result;
    
    std::regex assign_regex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)\s*$)");
    std::smatch assign_match;

    if (std::regex_match(code, assign_match, assign_regex))
    {
        std::string name = assign_match[1];
        std::string expr = assign_match[2];
        
        std::regex expr_regex(R"(^\s*(([A-Za-z_][A-Za-z0-9_]*|\d+)(\s*([\+\-\*\/])\s*([A-Za-z_][A-Za-z0-9_]*|\d+))?)\s*$)");
        std::smatch expr_match;

        if (std::regex_match(expr, expr_match, expr_regex))
        {
            std::string var1 = expr_match[2];
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
                    result.status = Equation::Status::kTypeError;
                    result.message = "Variable " + var1 + " is not an integer";
                    return result;
                }
            }
            else
            {
                result.status = Equation::Status::kNameError;
                result.message = "Variable " + var1 + " not found";
                return result;
            }

            if (!expr_match[4].matched)
            {
                result.status = Equation::Status::kSuccess;
                context->Set(name, val1);
                return result;
            }

            std::string op = expr_match[4];
            std::string var2 = expr_match[5];
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
                    result.status = Equation::Status::kTypeError;
                    result.message = "Variable " + var2 + " is not an integer";
                    return result;
                }
            }
            else
            {
                result.status = Equation::Status::kNameError;
                result.message = "Variable " + var2 + " not found";
                return result;
            }

            if (op == "+")
            {
                result.status = Equation::Status::kSuccess;
                context->Set(name, val1 + val2);
            }
            else if (op == "-")
            {
                result.status = Equation::Status::kSuccess;
                context->Set(name, val1 - val2);
            }
            else if (op == "*")
            {
                result.status = Equation::Status::kSuccess;
                context->Set(name, val1 * val2);
            }
            else if (op == "/")
            {
                if (val2 == 0)
                {
                    result.status = Equation::Status::kZeroDivisionError;
                    result.message = "Division by zero";
                }
                else
                {
                    result.status = Equation::Status::kSuccess;
                    context->Set(name, val1 / val2);
                }
            }
            else
            {
                result.status = Equation::Status::kAttributeError;
                result.message = "Invalid operator: " + op;
            }
        }
        else
        {
            result.status = Equation::Status::kSyntaxError;
            result.message = "Invalid expression syntax";
        }
    }
    else
    {
        result.status = Equation::Status::kSyntaxError;
        result.message = "Invalid assignment syntax. Expected: variable = expression";
    }
    
    return result;
}

//
class MockExprContext : public EquationContext
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
    EquationManagerTest() : manager_(std::unique_ptr<MockExprContext>(new MockExprContext()), ExecExpr, EquationParser::parseMultipleExpressions) {}

    void SetUp() override
    {
        manager_.Reset();
    }

    void VerifyVar(
        const Equation *var, Equation::Type type, Equation::Status status, const std::string &content,
        std::vector<std::string> dependencies = {}
    )
    {
        EXPECT_TRUE(var != nullptr);
        EXPECT_TRUE(var->type() == type);
        EXPECT_TRUE(var->status() == status);
        EXPECT_TRUE(var->content() == content);
        EXPECT_EQ(var->dependencies(), dependencies);
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

TEST_F(EquationManagerTest, AddEditRemoveEquation)
{
    manager_.AddEquation("A", "1");
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kInit, "A=1");
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
    EXPECT_THROW(manager_.AddEquation("A", "2"), DuplicateEquationNameError);

    manager_.AddEquation("B", "A+C");
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=A+C", {"A", "C"});
    VerifyNode(manager_.graph()->GetNode("B"), {"A"}, {});
    VerifyNode(manager_.graph()->GetNode("A"), {}, {"B"});
    VerifyEdges({{"B", "A"}, {"B", "C"}}, true);

    manager_.EditEquation("B", "B", "D");
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=D", {"D"});
    VerifyNode(manager_.graph()->GetNode("B"), {}, {});
    VerifyNode(manager_.graph()->GetNode("A"), {}, {});
    VerifyEdges({{"B", "D"}}, true);

    manager_.AddEquation("D", "C");
    EXPECT_TRUE(manager_.graph()->IsNodeExist("D"));
    VerifyNode(manager_.graph()->GetNode("B"), {"D"}, {});
    VerifyNode(manager_.graph()->GetNode("D"), {}, {"B"});
    VerifyEdges({{"B", "D"}, {"D", "C"}}, true);

    manager_.RemoveEquation("B");
    EXPECT_FALSE(manager_.graph()->IsNodeExist("B"));
    EXPECT_EQ(manager_.GetEquation("B"), nullptr);
    VerifyEdges({{"D", "C"}}, true);
    VerifyNode(manager_.graph()->GetNode("D"), {}, {});
}

TEST_F(EquationManagerTest, AddEditRemoveEquationStatement)
{
    std::string multiple_statements = "A=1;B=A+C";

    manager_.AddMultipleEquations(multiple_statements);
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kInit, "A=1");
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=A+C", {"A", "C"});
    VerifyNode(manager_.graph()->GetNode("B"), {"A"}, {});
    VerifyNode(manager_.graph()->GetNode("A"), {}, {"B"});
    VerifyEdges({{"B", "A"}, {"B", "C"}}, true);

    EXPECT_THROW(manager_.AddEquation("A", "2"), DuplicateEquationNameError);
    EXPECT_THROW(manager_.EditEquation("A", "C", "D"), std::runtime_error);
    EXPECT_THROW(manager_.RemoveEquation("A"), std::runtime_error);

    std::string new_statements = "C=A;B=A+C;E=1";
    EXPECT_THROW(manager_.AddMultipleEquations(new_statements), DuplicateEquationNameError);
    manager_.EditMultipleEquations(multiple_statements, new_statements);
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kInit, "C=A", {"A"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=A+C", {"A", "C"});
    VerifyVar(manager_.GetEquation("E"), Equation::Type::kVariable, Equation::Status::kInit, "E=1");
    EXPECT_FALSE(manager_.IsEquationExist("A"));
    VerifyNode(manager_.graph()->GetNode("B"), {"C"}, {});
    VerifyNode(manager_.graph()->GetNode("C"), {}, {"B"});
    VerifyNode(manager_.graph()->GetNode("E"), {}, {});
    VerifyEdges({{"C", "A"}, {"B", "C"}, {"B", "A"}}, true);

    EXPECT_THROW(manager_.AddMultipleEquations(multiple_statements), DuplicateEquationNameError);
    
    EXPECT_THROW(manager_.RemoveMultipleEquations(multiple_statements), std::runtime_error);
    manager_.RemoveMultipleEquations(new_statements);
    EXPECT_FALSE(manager_.IsEquationExist("A"));
    EXPECT_FALSE(manager_.IsEquationExist("B"));
    EXPECT_FALSE(manager_.IsEquationExist("C"));
    EXPECT_FALSE(manager_.IsEquationExist("E"));
}


TEST_F(EquationManagerTest, CycleDetection)
{
    std::string statements = "A=B*C;B=D;C=2";
    manager_.AddMultipleEquations(statements);    

    VerifyNode(manager_.graph()->GetNode("A"), {"B", "C"}, {});
    VerifyNode(manager_.graph()->GetNode("B"), {}, {"A"});
    VerifyNode(manager_.graph()->GetNode("C"), {}, {"A"});
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kInit, "A=B*C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=D", {"D"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kInit, "C=2");
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    EXPECT_THROW(manager_.EditEquation("B", "D", "B"), std::runtime_error);
    EXPECT_THROW(manager_.AddEquation("D", "A+B"), DependencyCycleException);
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kInit, "A=B*C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=D", {"D"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kInit, "C=2");
    EXPECT_EQ(manager_.GetEquation("D"), nullptr);
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}}, true);

    manager_.AddEquation("D", "E");
    EXPECT_THROW(manager_.AddEquation("E", "B"), DependencyCycleException);
    VerifyNode(manager_.graph()->GetNode("D"), {}, {"B"});
    VerifyVar(manager_.GetEquation("D"), Equation::Type::kVariable, Equation::Status::kInit, "D=E", {"E"});
    EXPECT_FALSE(manager_.IsEquationExist("E"));
    VerifyEdges({{"A", "B"}, {"A", "C"}, {"B", "D"}, {"D", "E"}}, true);
}

TEST_F(EquationManagerTest, UpdateContext)
{
    std::string statements0 = "A=B+C;B=D+E;C=F;D=1;F=10";
    manager_.AddMultipleEquations(statements0);
    manager_.AddEquation("E", "5");
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kInit, "A=B+C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kInit, "B=D+E", {"D", "E"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kInit, "C=F", {"F"});
    VerifyVar(manager_.GetEquation("D"), Equation::Type::kVariable, Equation::Status::kInit, "D=1", {});
    VerifyVar(manager_.GetEquation("E"), Equation::Type::kVariable, Equation::Status::kInit, "E=5", {});
    VerifyVar(manager_.GetEquation("F"), Equation::Type::kVariable, Equation::Status::kInit, "F=10", {});
    manager_.Update();
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kSuccess, "A=B+C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kSuccess, "B=D+E", {"D", "E"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kSuccess, "C=F", {"F"});
    VerifyVar(manager_.GetEquation("D"), Equation::Type::kVariable, Equation::Status::kSuccess, "D=1", {});
    VerifyVar(manager_.GetEquation("E"), Equation::Type::kVariable, Equation::Status::kSuccess, "E=5", {});
    VerifyVar(manager_.GetEquation("F"), Equation::Type::kVariable, Equation::Status::kSuccess, "F=10", {});
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

    std::string statements1 = "A=B+C;B=D+E;C=F;F=10";
    manager_.EditMultipleEquations(statements0,statements1);
    EXPECT_FALSE(manager_.context()->Contains("D"));
    EXPECT_FALSE(manager_.IsEquationExist("D"));
    manager_.Update();
    EXPECT_FALSE(manager_.context()->Contains("D"));
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kNameError, "A=B+C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kNameError, "B=D+E", {"D", "E"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kSuccess, "C=F", {"F"});
    VerifyVar(manager_.GetEquation("E"), Equation::Type::kVariable, Equation::Status::kSuccess, "E=5", {});
    VerifyVar(manager_.GetEquation("F"), Equation::Type::kVariable, Equation::Status::kSuccess, "F=10", {});
    EXPECT_FALSE(manager_.context()->Contains("A"));
    EXPECT_FALSE(manager_.context()->Contains("B"));
    EXPECT_TRUE(manager_.context()->Contains("C"));
    EXPECT_FALSE(manager_.context()->Contains("D"));
    EXPECT_TRUE(manager_.context()->Contains("E"));
    EXPECT_TRUE(manager_.context()->Contains("F"));
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);

    manager_.AddEquation("D", "E");
    std::string statements2 = "A=B+C;B=D+E;C=E+F;F=10";
    manager_.EditMultipleEquations(statements1, statements2);
    manager_.Update();
    VerifyVar(manager_.GetEquation("A"), Equation::Type::kVariable, Equation::Status::kSuccess, "A=B+C", {"B", "C"});
    VerifyVar(manager_.GetEquation("B"), Equation::Type::kVariable, Equation::Status::kSuccess, "B=D+E", {"D", "E"});
    VerifyVar(manager_.GetEquation("C"), Equation::Type::kVariable, Equation::Status::kSuccess, "C=E+F", {"E", "F"});
    VerifyVar(manager_.GetEquation("D"), Equation::Type::kVariable, Equation::Status::kSuccess, "D=E", {"E"});
    VerifyVar(manager_.GetEquation("E"), Equation::Type::kVariable, Equation::Status::kSuccess, "E=5", {});
    VerifyVar(manager_.GetEquation("F"), Equation::Type::kVariable, Equation::Status::kSuccess, "F=10", {});
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

    manager_.EditEquation("E", "E", "6");
    manager_.UpdateEquation("E");
    EXPECT_TRUE(manager_.context()->Get("A").Cast<int>() == 28);
    EXPECT_TRUE(manager_.context()->Get("B").Cast<int>() == 12);
    EXPECT_TRUE(manager_.context()->Get("C").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context()->Get("D").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context()->Get("E").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context()->Get("F").Cast<int>() == 10);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}