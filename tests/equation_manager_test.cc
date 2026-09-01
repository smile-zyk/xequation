#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_context.h"
#include "core/equation_group.h"
#include "core/equation_manager.h"

#include "gmock/gmock.h"
#include <regex>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unordered_set>

using namespace xequation;

void ParseDependencies(const std::string &expr, ParseResultItem &item)
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
    item.dependencies = res;
}

ParseResultItem ParseSingleStatement(const std::string &expr)
{
    ParseResultItem item;

    std::regex assign_regex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)\s*$)");
    std::smatch assign_match;

    if (std::regex_match(expr, assign_match, assign_regex))
    {
        std::string variable_name = assign_match[1].str();
        std::string expression = assign_match[2].str();

        item.name = variable_name;

        ParseDependencies(expression, item);
        item.content = expression;
        item.type = ItemType::kVariable;
    }
    else
    {
        throw ParseException("Syntax error: assignment operator '=' not found or variable name missing");
    }

    return item;
}

ParseResult ParseExpression(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kExpression;
    ParseResultItem item;
    item.name = "__expression__";
    item.content = code;
    item.type = ItemType::kUnknown;

    ParseDependencies(code, item);
    result.items.push_back(item);
    return result;
}

ParseResult ParseMultipleStatements(const std::string &input)
{
    ParseResult result;
    result.mode = ParseMode::kStatement;
    size_t start = 0;
    size_t end = 0;

    while (end != std::string::npos)
    {
        end = input.find(';', start);

        std::string expr = input.substr(start, (end == std::string::npos) ? std::string::npos : end - start);

        expr = std::regex_replace(expr, std::regex(R"(^\s+|\s+$)"), "");

        if (!expr.empty())
        {
            ParseResultItem item = ParseSingleStatement(expr);
            result.items.push_back(item);
        }

        if (end != std::string::npos)
        {
            start = end + 1;
        }
    }

    return result;
}

InterpretResult EvalExpr(const std::string &expr, EquationContext *context)
{
    InterpretResult result;
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
            EquationValue v1 = context->Get(var1);
            if (v1.IsInteger())
            {
                val1 = v1.Cast<int>();
            }
            else
            {
                result.status = ResultStatus::kTypeError;
                result.message = "Variable " + var1 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = ResultStatus::kNameError;
            result.message = "Variable " + var1 + " not found";
            return result;
        }

        if (!expr_match[4].matched)
        {
            result.status = ResultStatus::kSuccess;
            result.value = val1;
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
            EquationValue v2 = context->Get(var2);
            if (v2.IsInteger())
            {
                val2 = v2.Cast<int>();
            }
            else
            {
                result.status = ResultStatus::kTypeError;
                result.message = "Variable " + var2 + " is not an integer";
                return result;
            }
        }
        else
        {
            result.status = ResultStatus::kNameError;
            result.message = "Variable " + var2 + " not found";
            return result;
        }

        if (op == "+")
        {
            result.status = ResultStatus::kSuccess;
            result.value = val1 + val2;
        }
        else if (op == "-")
        {
            result.status = ResultStatus::kSuccess;
            result.value = val1 - val2;
        }
        else if (op == "*")
        {
            result.status = ResultStatus::kSuccess;
            result.value = val1 * val2;
        }
        else if (op == "/")
        {
            if (val2 == 0)
            {
                result.status = ResultStatus::kZeroDivisionError;
                result.message = "Division by zero";
            }
            else
            {
                result.status = ResultStatus::kSuccess;
                result.value = val1 / val2;
            }
        }
        else
        {
            result.status = ResultStatus::kAttributeError;
            result.message = "Invalid operator: " + op;
        }
    }
    else
    {
        result.status = ResultStatus::kSyntaxError;
        result.message = "Invalid expression syntax";
    }

    return result;
}

InterpretResult ExecExpr(const std::string &code, EquationContext *context)
{
    InterpretResult result;
    std::regex assign_regex(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)\s*$)");
    std::smatch assign_match;

    if (std::regex_match(code, assign_match, assign_regex))
    {
        std::string name = assign_match[1];
        std::string expr = assign_match[2];

        InterpretResult eval_result = EvalExpr(expr, context);

        context->Set(name, eval_result.value);
        result.status = eval_result.status;
        result.message = eval_result.message;
    }
    else
    {
        result.status = ResultStatus::kSyntaxError;
        result.message = "Invalid assignment syntax. Expected: variable = expression";
    }

    return result;
}

InterpretResult MockEvalHandler(const std::string &code, EquationContext *context)
{
    return EvalExpr(code, context);
}

InterpretResult MockExecHandler(const std::string &code, EquationContext *context)
{
    return ExecExpr(code, context);
}

ParseResult Parse(const std::string &code, ParseMode mode)
{
    if (mode == ParseMode::kExpression)
    {
        return ParseExpression(code);
    }
    else
    {
        return ParseMultipleStatements(code);
    }
}

class MockExprContext : public EquationContext
{
  public:
    virtual EquationValue Get(const std::string &var_name) const override
    {
        if (Contains(var_name))
        {
            return manager_.at(var_name);
        }
        return EquationValue::Null();
    }

    virtual void Set(const std::string &var_name, const EquationValue &value) override
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
    std::unordered_map<std::string, EquationValue> manager_;
};

class EquationManagerTest : public testing::Test
{
  protected:
    EquationManagerTest()
        : manager_(
              std::unique_ptr<MockExprContext>(new MockExprContext()), MockEvalHandler, MockExecHandler, Parse,
              EquationEngineInfo{"Mock"}
          )
    {
    }

    void SetUp() override
    {
        manager_.Reset();
    }

    EquationManager manager_;
};

TEST_F(EquationManagerTest, EquationGroupAddRemoveEditGet)
{
    ObjectId id_0 = manager_.AddEquationGroup("A=1");
    EXPECT_TRUE(manager_.IsEquationGroupExist(id_0));
    EXPECT_TRUE(manager_.IsEquationExist("A"));

    const EquationGroup *group_0 = manager_.GetEquationGroup(id_0);
    const Equation *equation_a = manager_.GetEquation("A");

    EXPECT_TRUE(group_0);
    EXPECT_EQ(group_0->id(), id_0);
    EXPECT_EQ(group_0->GetEquationNames(), std::vector<std::string>{"A"});
    EXPECT_EQ(group_0->manager(), &manager_);
    EXPECT_EQ(group_0->statement(), "A=1");

    EXPECT_TRUE(equation_a);
    EXPECT_TRUE(group_0->IsEquationExist("A"));
    EXPECT_TRUE(equation_a == group_0->GetEquation("A"));
    EXPECT_EQ(equation_a->name(), "A");
    EXPECT_EQ(equation_a->content(), "1");
    EXPECT_EQ(equation_a->group_id(), id_0);
    EXPECT_EQ(equation_a->manager(), &manager_);
    EXPECT_EQ(equation_a->message(), "");
    EXPECT_EQ(equation_a->type(), ItemType::kVariable);
    EXPECT_EQ(equation_a->status(), ResultStatus::kPending);

    manager_.EditEquationGroup(id_0, "A=2;B=A");
    EXPECT_TRUE(manager_.IsEquationExist("A"));
    EXPECT_TRUE(manager_.IsEquationExist("B"));

    EXPECT_TRUE(equation_a);
    EXPECT_TRUE(group_0->IsEquationExist("A"));
    EXPECT_TRUE(equation_a == group_0->GetEquation("A"));
    EXPECT_EQ(equation_a->name(), "A");
    EXPECT_EQ(equation_a->content(), "2");
    EXPECT_EQ(equation_a->group_id(), id_0);
    EXPECT_EQ(equation_a->manager(), &manager_);
    EXPECT_EQ(equation_a->message(), "");
    EXPECT_EQ(equation_a->type(), ItemType::kVariable);
    EXPECT_EQ(equation_a->status(), ResultStatus::kPending);

    const Equation *equation_b = manager_.GetEquation("B");
    EXPECT_TRUE(equation_b);
    EXPECT_TRUE(group_0->IsEquationExist("B"));
    EXPECT_TRUE(equation_b == group_0->GetEquation("B"));
    EXPECT_EQ(equation_b->name(), "B");
    EXPECT_EQ(equation_b->content(), "A");
    EXPECT_EQ(equation_b->group_id(), id_0);
    EXPECT_EQ(equation_b->manager(), &manager_);
    EXPECT_EQ(equation_b->message(), "");
    EXPECT_EQ(equation_b->type(), ItemType::kVariable);
    EXPECT_EQ(equation_b->status(), ResultStatus::kPending);

    manager_.EditEquationGroup(id_0, "B=3;C=B+1");
    EXPECT_FALSE(manager_.IsEquationExist("A"));
    EXPECT_TRUE(manager_.IsEquationExist("B"));
    EXPECT_TRUE(manager_.IsEquationExist("C"));

    EXPECT_TRUE(equation_b);
    EXPECT_TRUE(group_0->IsEquationExist("B"));
    EXPECT_TRUE(equation_b == group_0->GetEquation("B"));
    EXPECT_EQ(equation_b->name(), "B");
    EXPECT_EQ(equation_b->content(), "3");
    EXPECT_EQ(equation_b->group_id(), id_0);
    EXPECT_EQ(equation_b->manager(), &manager_);
    EXPECT_EQ(equation_b->message(), "");
    EXPECT_EQ(equation_b->type(), ItemType::kVariable);
    EXPECT_EQ(equation_b->status(), ResultStatus::kPending);

    const Equation *equation_c = manager_.GetEquation("C");
    EXPECT_TRUE(equation_c);
    EXPECT_TRUE(group_0->IsEquationExist("C"));
    EXPECT_TRUE(equation_c == group_0->GetEquation("C"));
    EXPECT_EQ(equation_c->name(), "C");
    EXPECT_EQ(equation_c->content(), "B+1");
    EXPECT_EQ(equation_c->group_id(), id_0);
    EXPECT_EQ(equation_c->manager(), &manager_);
    EXPECT_EQ(equation_c->message(), "");
    EXPECT_EQ(equation_c->type(), ItemType::kVariable);
    EXPECT_EQ(equation_c->status(), ResultStatus::kPending);

    ObjectId id_1 = manager_.AddEquationGroup("D=B+2;E=D+B");
    EXPECT_TRUE(manager_.IsEquationGroupExist(id_1));
    EXPECT_TRUE(manager_.IsEquationExist("D"));
    EXPECT_TRUE(manager_.IsEquationExist("E"));

    const EquationGroup *group_1 = manager_.GetEquationGroup(id_1);
    const Equation *equation_d = manager_.GetEquation("D");
    const Equation *equation_e = manager_.GetEquation("E");

    EXPECT_TRUE(group_1);
    EXPECT_EQ(group_1->id(), id_1);
    EXPECT_THAT(group_1->GetEquationNames(), ::testing::ElementsAre("D", "E"));

    EXPECT_TRUE(equation_d);
    EXPECT_TRUE(group_1->IsEquationExist("D"));
    EXPECT_TRUE(equation_d == group_1->GetEquation("D"));
    EXPECT_EQ(equation_d->name(), "D");
    EXPECT_EQ(equation_d->content(), "B+2");
    EXPECT_EQ(equation_d->group_id(), id_1);
    EXPECT_EQ(equation_d->manager(), &manager_);
    EXPECT_EQ(equation_d->message(), "");
    EXPECT_EQ(equation_d->type(), ItemType::kVariable);
    EXPECT_EQ(equation_d->status(), ResultStatus::kPending);

    EXPECT_TRUE(equation_e);
    EXPECT_TRUE(group_1->IsEquationExist("E"));
    EXPECT_TRUE(equation_e == group_1->GetEquation("E"));
    EXPECT_EQ(equation_e->name(), "E");
    EXPECT_EQ(equation_e->content(), "D+B");
    EXPECT_EQ(equation_e->group_id(), id_1);
    EXPECT_EQ(equation_e->manager(), &manager_);
    EXPECT_EQ(equation_e->message(), "");
    EXPECT_EQ(equation_e->type(), ItemType::kVariable);
    EXPECT_EQ(equation_e->status(), ResultStatus::kPending);

    manager_.RemoveEquationGroup(id_1);
    EXPECT_FALSE(manager_.IsEquationGroupExist(id_1));
    EXPECT_FALSE(manager_.IsEquationExist("D"));
    EXPECT_FALSE(manager_.IsEquationExist("E"));
}

TEST_F(EquationManagerTest, EquationException)
{
    auto id = manager_.AddEquationGroup("A=1;B=2");
    manager_.AddEquationGroup("C=3");
    try
    {
        auto tmp_id = manager_.AddEquationGroup("A=3");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationAlreayExists);
        EXPECT_EQ(e.equation_name(), "A");
    }

    try
    {
        manager_.EditEquationGroup(id, "C=2");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationAlreayExists);
        EXPECT_EQ(e.equation_name(), "C");
    }

    manager_.RemoveEquationGroup(id);

    try
    {
        manager_.EditEquationGroup(id, "C=1");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationGroupNotFound);
        EXPECT_EQ(e.group_id(), id);
    }

    try
    {
        manager_.RemoveEquationGroup(id);
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationGroupNotFound);
        EXPECT_EQ(e.group_id(), id);
    }
    try
    {
        manager_.UpdateEquation("E");
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationNotFound);
        EXPECT_EQ(e.equation_name(), "E");
    }
    try
    {
        manager_.UpdateEquationGroup(id);
        FAIL();
    }
    catch (const EquationException &e)
    {
        EXPECT_EQ(e.error_code(), EquationException::ErrorCode::kEquationGroupNotFound);
        EXPECT_EQ(e.group_id(), id);
    }
}

TEST_F(EquationManagerTest, EquationManagerUpdate)
{
    ObjectId id_0 = manager_.AddEquationGroup("A=B+C;B=D+E;C=F;D=1;F=10");
    ObjectId id_1 = manager_.AddEquationGroup("E=5");
    manager_.Update();
    EXPECT_TRUE(manager_.context().Contains("A"));
    EXPECT_TRUE(manager_.context().Contains("B"));
    EXPECT_TRUE(manager_.context().Contains("C"));
    EXPECT_TRUE(manager_.context().Contains("D"));
    EXPECT_TRUE(manager_.context().Contains("E"));
    EXPECT_TRUE(manager_.context().Contains("F"));
    EXPECT_TRUE(manager_.context().Get("A").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("D").Cast<int>() == 1);
    EXPECT_TRUE(manager_.context().Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("F").Cast<int>() == 10);

    manager_.EditEquationGroup(id_0, "A=B+C;B=D+E;C=F;F=10");
    EXPECT_FALSE(manager_.context().Contains("D"));
    manager_.Update();
    EXPECT_FALSE(manager_.context().Contains("A"));
    EXPECT_FALSE(manager_.context().Contains("B"));
    EXPECT_TRUE(manager_.context().Contains("C"));
    EXPECT_FALSE(manager_.context().Contains("D"));
    EXPECT_TRUE(manager_.context().Contains("E"));
    EXPECT_TRUE(manager_.context().Contains("F"));
    EXPECT_TRUE(manager_.context().Get("A").IsNull());
    EXPECT_TRUE(manager_.context().Get("B").IsNull());
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("D").IsNull());
    EXPECT_TRUE(manager_.context().Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("F").Cast<int>() == 10);

    manager_.AddEquationGroup("D=E");
    manager_.UpdateEquation("D");
    EXPECT_TRUE(manager_.context().Contains("A"));
    EXPECT_TRUE(manager_.context().Contains("B"));
    EXPECT_TRUE(manager_.context().Contains("C"));
    EXPECT_TRUE(manager_.context().Contains("D"));
    EXPECT_TRUE(manager_.context().Contains("E"));
    EXPECT_TRUE(manager_.context().Contains("F"));
    EXPECT_TRUE(manager_.context().Get("A").Cast<int>() == 20);
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("D").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("F").Cast<int>() == 10);

    manager_.EditEquationGroup(id_0, "A=B+C;B=D+E;C=E+F;F=10");
    manager_.UpdateEquationGroup(id_0);
    EXPECT_TRUE(manager_.context().Get("A").Cast<int>() == 25);
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 15);
    EXPECT_TRUE(manager_.context().Get("D").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("F").Cast<int>() == 10);
}

TEST_F(EquationManagerTest, Eval) 
{
    ObjectId id_0 = manager_.AddEquationGroup("A=B+C;B=D+E;C=F;D=1;E=5;F=10");
    manager_.UpdateEquationGroup(id_0);
    InterpretResult res = manager_.Eval("A+B");

    EXPECT_EQ(res.value.Cast<int>(), 22);
    EXPECT_EQ(res.status, ResultStatus::kSuccess);

    res = manager_.Eval("G+1");
    EXPECT_TRUE(res.value.IsNull());
    EXPECT_EQ(res.status, ResultStatus::kNameError);
}

TEST_F(EquationManagerTest, RenameDependencyAcrossGroupsDoesNotUpdateDependent)
{
    // User scenario: AddEquation(a,1) / AddEquation(b,a) in two groups;
    // after renaming "a" to "c" in group 0, UpdateEquationGroup(group 0) must also
    // recompute "b" in group 1.
    ObjectId id_0 = manager_.AddEquationGroup("A=1");
    ObjectId id_1 = manager_.AddEquationGroup("B=A");
    manager_.Update();

    EXPECT_TRUE(manager_.context().Contains("B"));
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 1);
    EXPECT_EQ(manager_.GetEquation("B")->status(), ResultStatus::kSuccess);

    // Rename A=1 to C=1 (equivalent to editing group 0: remove "a", add "c").
    manager_.EditEquationGroup(id_0, "C=1");

    // After the edit, "B"'s dependency "a" is gone, so "B" must be dirty.
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());

    // Equivalent to the GUI edit flow: only the edited group is updated.
    // Correct behavior: "B"'s dependency "a" no longer exists, so "B" must be
    // recomputed -> NameError and removed from context, instead of keeping the stale
    // value 1 and Success status.
    manager_.UpdateEquationGroup(id_0);

    EXPECT_FALSE(manager_.context().Contains("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status(), ResultStatus::kNameError);
    EXPECT_TRUE(manager_.context().Contains("C"));
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 1);
}

TEST_F(EquationManagerTest, UpdateEquationAfterRenameRecomputesDirtyDependents)
{
    // User scenario: A=1 (group 0), B=A (group 1). After renaming "A" to "C" in group 0,
    // even an explicit UpdateEquation("C") must recompute the invalidated "B";
    // it must not keep the stale value 1 and Success status.
    ObjectId id_0 = manager_.AddEquationGroup("A=1");
    ObjectId id_1 = manager_.AddEquationGroup("B=A");
    manager_.Update();

    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 1);

    manager_.EditEquationGroup(id_0, "C=1");

    // The user now updates the new node C ("B"'s dependency "a" is gone and dirty).
    manager_.UpdateEquation("C");

    EXPECT_FALSE(manager_.context().Contains("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status(), ResultStatus::kNameError);
    EXPECT_TRUE(manager_.context().Contains("C"));
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 1);
}

TEST_F(EquationManagerTest, ResetContextThenUpdateRecoversAllValues)
{
    // Fallback scenario for the clear semantics: after ResetContext() clears the context,
    // Update() must recompute everything and restore the values, not no-op because the
    // nodes are no longer dirty.
    ObjectId id_0 = manager_.AddEquationGroup("A=B+C;B=D+E;C=F;D=1;E=5;F=10");
    manager_.Update();

    EXPECT_TRUE(manager_.context().Get("A").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 10);

    // After a successful recompute the nodes should be clean.
    EXPECT_FALSE(manager_.graph().GetNode("A")->dirty_flag());
    EXPECT_FALSE(manager_.graph().GetNode("B")->dirty_flag());

    manager_.ResetContext();

    // The context is cleared and every node should be re-dirtied.
    EXPECT_FALSE(manager_.context().Contains("A"));
    EXPECT_TRUE(manager_.graph().GetNode("A")->dirty_flag());
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());
    EXPECT_TRUE(manager_.graph().GetNode("F")->dirty_flag());

    manager_.Update();

    // Everything is restored.
    EXPECT_TRUE(manager_.context().Get("A").Cast<int>() == 16);
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 6);
    EXPECT_TRUE(manager_.context().Get("C").Cast<int>() == 10);
    EXPECT_TRUE(manager_.context().Get("D").Cast<int>() == 1);
    EXPECT_TRUE(manager_.context().Get("E").Cast<int>() == 5);
    EXPECT_TRUE(manager_.context().Get("F").Cast<int>() == 10);
    EXPECT_EQ(manager_.GetEquation("A")->status(), ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, UpdateEquationStatusThenUpdateRecovers)
{
    // After UpdateEquationStatus removes the context value, Update() must recompute it.
    ObjectId id_0 = manager_.AddEquationGroup("A=1;B=A");
    manager_.Update();

    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 1);

    // Simulate an interruption: "B" is marked KeyBoardInterrupt and removed from context.
    manager_.UpdateEquationStatus("B", ResultStatus::kKeyBoardInterrupt);
    EXPECT_FALSE(manager_.context().Contains("B"));
    EXPECT_EQ(manager_.GetEquation("B")->status(), ResultStatus::kKeyBoardInterrupt);
    EXPECT_TRUE(manager_.graph().GetNode("B")->dirty_flag());

    // A later Update must recompute "B".
    manager_.Update();
    EXPECT_TRUE(manager_.context().Get("B").Cast<int>() == 1);
    EXPECT_EQ(manager_.GetEquation("B")->status(), ResultStatus::kSuccess);
    EXPECT_FALSE(manager_.graph().GetNode("B")->dirty_flag());
}

// ============================================================================
// Registered expressions (只算不存)
// ============================================================================

TEST_F(EquationManagerTest, ExpressionRegisterAndEvaluate)
{
    // Expression is registered, evaluated with Eval semantics, and its result is
    // cached in the Expression object -- never written into the context.
    manager_.AddEquationGroup("A=1;B=2");
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("A+B");
    EXPECT_TRUE(manager_.IsExpressionExist(expr_id));
    EXPECT_EQ(manager_.GetExpressionIds().size(), 1u);

    const Expression *expr = manager_.GetExpression(expr_id);
    ASSERT_NE(expr, nullptr);
    EXPECT_EQ(expr->content, "A+B");
    EXPECT_THAT(expr->dependencies, testing::UnorderedElementsAre("A", "B"));

    // Not evaluated yet.
    EXPECT_TRUE(manager_.GetExpressionValue(expr_id).IsNull());

    manager_.UpdateExpression(expr_id);
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 3);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);

    // The expression is not a symbol: it must not appear in equation APIs or in
    // the context.  Only the two equations A, B are listed.
    EXPECT_THAT(manager_.GetEquationNames(), ::testing::ElementsAre("A", "B"));
    EXPECT_FALSE(manager_.context().Contains("A+B"));
}

TEST_F(EquationManagerTest, ExpressionRecomputesWhenEquationChanges)
{
    // The expression depends on equation A; editing A and running Update() must
    // recompute the expression automatically.
    ObjectId id_0 = manager_.AddEquationGroup("A=1");
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("A*2");
    manager_.Update();
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 2);

    manager_.EditEquationGroup(id_0, "A=5");
    manager_.Update();
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 10);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, ExpressionRecomputesOnUpdateEquation)
{
    // A registered expression that depends on equation A must be recomputed when a
    // dependent equation is edited AND updated via the single-equation path
    // (UpdateEquation) -- not only through Update()/UpdateEquationGroup().
    ObjectId id_0 = manager_.AddEquationGroup("A=1;H=A");
    manager_.Update();

    ObjectId expr_id = manager_.AddExpression("H*2");
    manager_.Update();
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 2);

    // Edit `A` and update the single equation `A`: the change propagates to `H`,
    // then to the expression -- the expression must be recomputed too.
    manager_.EditEquationGroup(id_0, "A=7;H=A");
    manager_.UpdateEquation("A");
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 14);
    EXPECT_EQ(manager_.GetExpression(expr_id)->result.status, ResultStatus::kSuccess);
}

TEST_F(EquationManagerTest, ExpressionFailureStaysDirtyThenRecovers)
{
    // "X" is undefined at registration: the expression fails with NameError and
    // stays dirty.  Defining X and updating must recover it.
    ObjectId expr_id = manager_.AddExpression("X+1");
    const Expression *expr = manager_.GetExpression(expr_id);
    ASSERT_NE(expr, nullptr);

    manager_.Update();
    EXPECT_EQ(expr->result.status, ResultStatus::kNameError);

    // The expression's graph slot is the only node that is not an equation.
    std::string expr_node_name;
    manager_.graph().Traversal([&](const std::string &node_name) {
        if (!manager_.IsEquationExist(node_name))
        {
            expr_node_name = node_name;
        }
    });
    ASSERT_FALSE(expr_node_name.empty());
    EXPECT_TRUE(manager_.graph().GetNode(expr_node_name)->dirty_flag());

    manager_.AddEquationGroup("X=2");
    manager_.Update();
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 3);
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
    // The expression's graph node is removed with it; only the passive edge to
    // the unresolved dependency "A" may remain, and no node survives.
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

    // Duplicate registration and conflicts with equation names are rejected.
    EXPECT_FALSE(manager_.AddExternalInput("c"));
    manager_.AddEquationGroup("a=1");
    EXPECT_FALSE(manager_.AddExternalInput("a"));

    manager_.RemoveExternalInput("c");
    EXPECT_FALSE(manager_.IsExternalInput("c"));
    EXPECT_THAT(manager_.GetExternalInputNames(), testing::Not(testing::Contains("c")));
}

TEST_F(EquationManagerTest, ExternalInputRegisteredBeforeEquation)
{
    // Register "c" first, then an equation that depends on it.
    EXPECT_TRUE(manager_.AddExternalInput("c"));
    manager_.AddEquationGroup("x=c*2");
    manager_.Update();

    // "c" is not in the context -> x is a NameError until injected.
    EXPECT_EQ(manager_.GetEquation("x")->status(), ResultStatus::kNameError);

    // Inject a transient value: dependents are recomputed, then the name is
    // removed again (it never persists in the context).
    manager_.UpdateExternalInput("c", EquationValue(3));
    EXPECT_EQ(manager_.context().Get("x").Cast<int>(), 6);
    EXPECT_FALSE(manager_.context().Contains("c"));
}

TEST_F(EquationManagerTest, ExternalInputRegisteredAfterEquation)
{
    // Dependency edges stay inactive until both endpoints exist: registering the
    // external input after the equation activates the edge.
    manager_.AddEquationGroup("x=c*2");
    EXPECT_TRUE(manager_.AddExternalInput("c"));
    manager_.Update();

    EXPECT_EQ(manager_.GetEquation("x")->status(), ResultStatus::kNameError);

    manager_.UpdateExternalInput("c", EquationValue(4));
    EXPECT_EQ(manager_.context().Get("x").Cast<int>(), 8);
    EXPECT_FALSE(manager_.context().Contains("c"));
}

TEST_F(EquationManagerTest, ExternalInputWithoutValueInvalidatesOnly)
{
    // Dataset-like scenario: the value lives outside the context (host owns it).
    // UpdateExternalInput without a value only invalidates/propagates; the actual
    // recompute happens on a later Update().
    manager_.AddExternalInput("c");
    manager_.AddEquationGroup("x=c+1");
    manager_.Update();
    EXPECT_EQ(manager_.GetEquation("x")->status(), ResultStatus::kNameError);

    // Host provides the value outside the manager; manager only invalidates.
    manager_.context().Set("c", EquationValue(10));
    manager_.UpdateExternalInput("c");
    EXPECT_TRUE(manager_.graph().GetNode("x")->dirty_flag());

    manager_.Update();
    EXPECT_EQ(manager_.context().Get("x").Cast<int>(), 11);
}

TEST_F(EquationManagerTest, ExternalInputPropagatesToExpressions)
{
    // Expressions that (transitively) depend on an external input are recomputed
    // when the input is updated.
    manager_.AddExternalInput("c");
    manager_.AddEquationGroup("x=c+1");
    ObjectId expr_id = manager_.AddExpression("x*2");
    manager_.Update();
    EXPECT_EQ(manager_.GetEquation("x")->status(), ResultStatus::kNameError);

    manager_.UpdateExternalInput("c", EquationValue(4));
    EXPECT_EQ(manager_.context().Get("x").Cast<int>(), 5);
    EXPECT_EQ(manager_.GetExpressionValue(expr_id).Cast<int>(), 10);
    EXPECT_FALSE(manager_.context().Contains("c"));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}