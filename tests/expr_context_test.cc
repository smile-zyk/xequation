#include "expr_common.h"
#include "expr_context.h"
#include "variable.h"

#include <regex>
#include <string>

#include <gtest/gtest.h>

using namespace xexprengine;

//
class MockExprContext : public ExprContext
{
  public:
    MockExprContext() : ExprContext()
    {
        set_parse_callback([this](const std::string &expr) { return this->ParseCallback(expr); });
        set_evaluate_callback([this](const std::string &expr, const ExprContext *ctx) {
            return this->EvaluateCallback(expr, ctx);
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

    ParseResult ParseCallback(const std::string &expr)
    {
        // use regular expression to parse variables
        // only support two variables and one operator
        // literal only support integer
        // varible name only support A-Z
        // check format: VAR OP VAR
        // if A + 1 only add A to dependency
        ParseResult result;
        std::regex expr_regex(R"(^\s*([A-Z]+|\d+)\s*([\+\-\*\/])\s*([A-Z]+|\d+)\s*$)");
        std::smatch match;
        // if A + 1 only add A to dependency
        if (std::regex_match(expr, match, expr_regex))
        {
            // extract variables and operator
            if (std::regex_match(match[1].str(), std::regex(R"(^[A-Z]+$)")))
            {
                result.variables.insert(match[1]);
            }
            if (std::regex_match(match[3].str(), std::regex(R"(^[A-Z]+$)")))
            {
                result.variables.insert(match[3]);
            }
            result.status = xexprengine::VariableStatus::kExprParseSuccess;
        }
        else
        {
            result.status = xexprengine::VariableStatus::kExprParseSyntaxError;
        }
        return result;
    }

    EvalResult EvaluateCallback(const std::string &expr, const ExprContext *context)
    {
        EvalResult result;
        std::regex expr_regex(R"(^\s*([A-Z]+|\d+)\s*([\+\-\*\/])\s*([A-Z]+|\d+)\s*$)");
        std::smatch match;
        if (std::regex_match(expr, match, expr_regex))
        {
            // extract variables and operator
            std::string var1 = match[1];
            std::string op = match[2];
            std::string var2 = match[3];

            int val1 = 0;
            int val2 = 0;

            // get value of var1
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

            // get value of var2
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

            // perform calculation
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

    MockExprContext context_;
};

TEST_F(ExprContextTest, BasicOperation)
{
    context_.SetValue("A", 1);
    EXPECT_TRUE(context_.IsVariableExist("A"));
    EXPECT_TRUE(context_.GetVariable("A")->GetType() == Variable::Type::Raw);
    EXPECT_TRUE(context_.GetVariable("A")->As<RawVariable>()->value().Cast<int>() == 1);
    EXPECT_FALSE(context_.IsContextValueExist("A"));
    context_.Update();
    EXPECT_TRUE(context_.IsVariableExist("A"));
    EXPECT_TRUE(context_.GetContextValue("A").Cast<int>() == 1);
    EXPECT_TRUE(context_.GetVariable("A")->status() == xexprengine::VariableStatus::kRawVar);

    context_.RemoveVariable("A");
    EXPECT_FALSE(context_.IsVariableExist("A"));
    EXPECT_FALSE(context_.IsContextValueExist("A"));

    context_.SetExpression("B", "A + 1");
    EXPECT_TRUE(context_.IsVariableExist("B"));
    EXPECT_TRUE(context_.GetVariable("B")->GetType() == Variable::Type::Expr);
    EXPECT_TRUE(context_.GetVariable("B")->As<ExprVariable>()->expression() == "A + 1");
    context_.Update();
    EXPECT_FALSE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.GetVariable("B")->status() == xexprengine::VariableStatus::kMissingDependency);

    context_.SetValue("A", 10);
    context_.Update();
    EXPECT_TRUE(context_.IsContextValueExist("A"));
    EXPECT_TRUE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.GetVariable("B")->status() == xexprengine::VariableStatus::kExprEvalSuccess);
    EXPECT_TRUE(context_.GetContextValue("A").Cast<int>() == 10);
    EXPECT_TRUE(context_.GetContextValue("B").Cast<int>() == 11);

    context_.SetExpression("C", "A +");
    context_.Update();
    EXPECT_FALSE(context_.IsContextValueExist("C"));
    EXPECT_TRUE(context_.GetVariable("C")->status() == xexprengine::VariableStatus::kExprParseSyntaxError);

    context_.SetExpression("C", "A +B");
    context_.Update();
    EXPECT_TRUE(context_.IsContextValueExist("C"));
    EXPECT_TRUE(context_.GetVariable("C")->status() == xexprengine::VariableStatus::kExprEvalSuccess);
    EXPECT_TRUE(context_.GetContextValue("C").Cast<int>() == 21);

    context_.SetExpression("A", "D + E");
    context_.Update();
    EXPECT_FALSE(context_.IsContextValueExist("A"));
    EXPECT_FALSE(context_.IsContextValueExist("B"));
    EXPECT_TRUE(context_.GetVariable("B")->status() == xexprengine::VariableStatus::kExprEvalNameError);
    EXPECT_FALSE(context_.IsContextValueExist("C"));
    EXPECT_TRUE(context_.GetVariable("C")->status() == xexprengine::VariableStatus::kExprEvalNameError);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}