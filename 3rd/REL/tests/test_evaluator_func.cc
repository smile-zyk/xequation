// Custom function tests: registration, default parameter slots, dispatch.

#include "rel.h"
#include "environment.h"

#include "measurement.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    // f(x, y = 10, z = 100) -> x + y + z
    rel::Function make_sum_with_defaults()
    {
        return rel::Function(
            "f",
            std::vector<rel::FunctionParam>{
                rel::Param("x"),
                rel::Param("y", rel::Value::Integer(10)),
                rel::Param("z", rel::Value::Integer(100)),
            },
            [](const rel::Function::ArgMap& args) -> rel::Value {
                int sum = args.at("x").as_measurement().as_scalar<int>()
                        + args.at("y").as_measurement().as_scalar<int>()
                        + args.at("z").as_measurement().as_scalar<int>();
                return rel::Value::Integer(sum);
            });
    }
} // namespace

// =========================================================================
//  Basic dispatch
// =========================================================================

TEST(FunctionTest, CallWithAllArgs)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    rel::Value v = rel::Eval("f(1, 2, 3)", &env);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 6);
}

TEST(FunctionTest, CallWithDefaultParams)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    rel::Value v = rel::Eval("f(1)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 111);  // 1 + 10 + 100
}

TEST(FunctionTest, EmptyCallFillsAllDefaults)
{
    // A function whose every parameter has a default can be called as f().
    rel::Environment env;
    std::vector<rel::FunctionParam> params;
    params.push_back(rel::FunctionParam("a", rel::Value::Integer(1)));
    params.push_back(rel::FunctionParam("b", rel::Value::Integer(2)));
    rel::Environment::RegisterFunction(rel::Function(
        "g",
        std::move(params),
        [](const rel::Function::ArgMap& args) -> rel::Value {
            int sum = args.at("a").as_measurement().as_scalar<int>()
                    + args.at("b").as_measurement().as_scalar<int>();
            return rel::Value::Integer(sum);
        }));

    rel::Value v = rel::Eval("g()", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 3);  // 1 + 2
}

TEST(FunctionTest, RequiredParamEmptyCallThrows)
{
    // f() with a required first parameter must throw.
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    EXPECT_THROW(rel::Eval("f()", &env), std::runtime_error);
}

// =========================================================================
//  Default argument slots (缺省参数�?
// =========================================================================

TEST(FunctionTest, SkippedMiddleSlot)
{
    // f(1, , 3): y slot omitted -> default 10
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    rel::Value v = rel::Eval("f(1, , 3)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 14);  // 1 + 10 + 3
}

TEST(FunctionTest, SkippedLeadingSlots)
{
    // f(, , 3): x has no default -> must throw at evaluation.
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    EXPECT_THROW(rel::Eval("f(, , 3)", &env), std::runtime_error);
}

TEST(FunctionTest, TrailingDefaultSlotsRejectedAtParse)
{
    // f(1, , ) is a parse error (trailing default slots not allowed).
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    EXPECT_THROW(rel::Eval("f(1, , )", &env), std::runtime_error);
}

TEST(FunctionTest, ExpressionArgsWithSkip)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    rel::Value v = rel::Eval("f(1 + 2, , 3 * 4)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 25);  // 3 + 10 + 12
}

TEST(FunctionTest, FunctionInsideExpression)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    rel::Value v = rel::Eval("f(1, 2) * 2 + 1", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 207);  // (1+2+100)*2+1 = 207
}

// =========================================================================
//  Validation
// =========================================================================

TEST(FunctionTest, MissingRequiredArgThrows)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    EXPECT_THROW(rel::Eval("f(, 2, 3)", &env), std::runtime_error);
}

TEST(FunctionTest, TooManyArgsThrows)
{
    rel::Environment env;
    rel::Environment::RegisterFunction(make_sum_with_defaults());
    EXPECT_THROW(rel::Eval("f(1, 2, 3, 4)", &env), std::runtime_error);
}

TEST(FunctionTest, UnknownFunctionThrows)
{
    rel::Environment env;
    EXPECT_THROW(rel::Eval("nosuchfn(1)", &env), std::runtime_error);
}

// =========================================================================
//  Coexistence with matrix indexing
// =========================================================================

TEST(FunctionTest, MatrixIndexStillWorks)
{
    rel::Value v = rel::Eval("{10, 20, 30}(2)");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 20);  // 1-based index 2
}

// =========================================================================
//  Convenience API (rel::RegisterFunction)
// =========================================================================

TEST(FunctionTest, ConvenienceRegisterApi)
{
    rel::Environment env;

    std::vector<rel::FunctionParam> params;
    params.push_back(rel::FunctionParam("a"));
    params.push_back(rel::FunctionParam("b", rel::Value::Integer(5)));

    rel::Environment::RegisterFunction(rel::Function(
        "muladd",
        std::move(params),
        [](const rel::Function::ArgMap& args) -> rel::Value {
            int a = args.at("a").as_measurement().as_scalar<int>();
            int b = args.at("b").as_measurement().as_scalar<int>();
            return rel::Value::Integer(a * b);
        }));

    rel::Value v = rel::Eval("muladd(3)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 15);  // 3 * 5

    rel::Value v2 = rel::Eval("muladd(3, 4)", &env);
    EXPECT_EQ(v2.as_measurement().as_scalar<int>(), 12);
}

// =========================================================================
//  Re-registration shadows previous implementation
// =========================================================================

TEST(FunctionTest, ReregisterOverwrites)
{
    rel::Environment env;

    rel::Environment::RegisterFunction(rel::Function(
        "g", std::vector<rel::FunctionParam>{}, 
        [](const rel::Function::ArgMap&) -> rel::Value {
            return rel::Value::Integer(1);
        }));
    rel::Environment::RegisterFunction(rel::Function(
        "g", std::vector<rel::FunctionParam>{},
        [](const rel::Function::ArgMap&) -> rel::Value {
            return rel::Value::Integer(2);
        }));

    rel::Value v = rel::Eval("g()", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 2);
}
