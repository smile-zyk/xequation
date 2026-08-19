// Evaluator step-1 tests: literals and boolean expressions.

#include "evaluator.h"
#include "environment.h"
#include "rel.h"

#include "expr.h"

#include "data_array.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{
    // ---- helpers ------------------------------------------------------------

    rel::Value eval_expr(const rel::Expr& expr)
    {
        rel::Environment env;
        rel::Evaluator e(env);
        return e.Evaluate(expr);
    }

    rel::Environment make_env_with_builtins()
    {
        rel::Environment env;
        rel::Environment::InitBuiltinConstants();
        return env;
    }
} // namespace

// =========================================================================
//  Boolean literal
// =========================================================================

TEST(EvaluatorLitTest, BooleanTrue)
{
    rel::BooleanExpr expr(1, 1, true);
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kBoolean);
    EXPECT_EQ(v.as_measurement().as_scalar<bool>(), true);
}

TEST(EvaluatorLitTest, BooleanFalse)
{
    rel::BooleanExpr expr(1, 1, false);
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kBoolean);
    EXPECT_EQ(v.as_measurement().as_scalar<bool>(), false);
}

// =========================================================================
//  Number: plain integer
// =========================================================================

TEST(EvaluatorLitTest, PlainInteger)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Integer, "42", 10, "");
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kScalar);
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 42);
}

TEST(EvaluatorLitTest, PlainReal)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "3.14", 10, "");
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.14);
}

TEST(EvaluatorLitTest, HexInteger)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Integer, "0x1F", 16, "");
    rel::Value v = eval_expr(expr);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 31);  // 0x1F = 31
}

TEST(EvaluatorLitTest, OctalInteger)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Integer, "0377", 8, "");
    rel::Value v = eval_expr(expr);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 255);  // 0377 = 255
}

// =========================================================================
//  Number: with units
// =========================================================================

TEST(EvaluatorLitTest, IntegerWithUnit)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Integer, "50", 10, "Ohm");
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 50);
    EXPECT_TRUE(v.as_measurement().unit().has_dimension());
}

TEST(EvaluatorLitTest, RealWithUnit)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "1.23", 10, "Hz");
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.23);
}

// =========================================================================
//  Number: scale factor + unit combined (Unit::parse handles all)
// =========================================================================

TEST(EvaluatorLitTest, MegaScale)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Integer, "8", 10, "M");
    rel::Value v = eval_expr(expr);
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 8);
    // "M" is a dimensionless scale factor; canonicalized absorbs 1e6.
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 8e6);
}

TEST(EvaluatorLitTest, MilliScale)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "5", 10, "m");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 5.0);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 5e-3);
}

TEST(EvaluatorLitTest, KiloScale)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "2.5", 10, "K");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 2.5);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 2500.0);
}

TEST(EvaluatorLitTest, GigaHz)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "1.23", 10, "GHz");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.23);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 1.23e9);
}

TEST(EvaluatorLitTest, MilliVolt)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "3.3", 10, "mV");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.3);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 3.3e-3);
}

TEST(EvaluatorLitTest, PredefCentimeter)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "1.0", 10, "cm");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.0);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 0.01);
}

TEST(EvaluatorLitTest, PredefPHZ)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "1.0", 10, "PHz");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.0);
    EXPECT_DOUBLE_EQ(v.as_measurement().canonicalized().as_scalar<double>(), 1e15);
}

TEST(EvaluatorLitTest, PredefDB)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Real, "3.0", 10, "dB");
    rel::Value v = eval_expr(expr);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.0);
}

// =========================================================================
//  Number: imaginary
// =========================================================================

TEST(EvaluatorLitTest, Imaginary)
{
    rel::NumberExpr expr(1, 1, rel::NumberKind::Imaginary, "3.5i", 10, "");
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kComplex);
}

// =========================================================================
//  String literal
// =========================================================================

TEST(EvaluatorLitTest, PlainString)
{
    rel::StringExpr expr(1, 1, "hello", false);
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kString);
    EXPECT_EQ(v.as_measurement().as_scalar<std::string>(), "hello");
}

TEST(EvaluatorLitTest, RawString)
{
    rel::StringExpr expr(1, 1, "no\\escape", true);
    rel::Value v = eval_expr(expr);
    EXPECT_EQ(v.as_measurement().as_scalar<std::string>(), "no\\escape");
}

TEST(EvaluatorLitTest, ReferenceNeedsEnv)
{
    // Reference to an undefined name throws.
    std::vector<rel::RefSegment> segs;
    segs.push_back(rel::RefSegment("x", rel::RefSeparator::None));
    rel::ReferenceExpr expr(1, 1, segs);
    EXPECT_THROW(eval_expr(expr), std::runtime_error);
}
