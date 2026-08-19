// Evaluator operator tests -> uses eval for concise test setup.

#include "rel.h"

#include "data_array.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <string>

using rel::Eval;

// =========================================================================
//  Arithmetic
// =========================================================================

TEST(OperatorTest, Addition)
{
    rel::Value v = Eval("1 + 2");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 3);
}

TEST(OperatorTest, Subtraction)
{
    rel::Value v = Eval("5 - 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 2);
}

TEST(OperatorTest, Multiplication)
{
    rel::Value v = Eval("3 * 4");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 12);
}

TEST(OperatorTest, Division)
{
    rel::Value v = Eval("10 / 2");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 5.0);
}

TEST(OperatorTest, Modulo)
{
    rel::Value v = Eval("7 % 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, Power)
{
    rel::Value v = Eval("2 ** 3");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 8.0);
}

TEST(OperatorTest, ChainedAddition)
{
    rel::Value v = Eval("1 + 2 + 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 6);
}

TEST(OperatorTest, PrecedenceMulBeforeAdd)
{
    rel::Value v = Eval("2 * 3 + 4");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 10);
}

TEST(OperatorTest, GroupingOverridesPrecedence)
{
    rel::Value v = Eval("(1 + 2) * 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 9);
}

TEST(OperatorTest, RealDivision)
{
    rel::Value v = Eval("10.0 / 3.0");
    EXPECT_TRUE(v.is_measurement());
}

// =========================================================================
//  Shift
// =========================================================================

TEST(OperatorTest, LeftShift)
{
    rel::Value v = Eval("1 << 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 4);
}

TEST(OperatorTest, RightShift)
{
    rel::Value v = Eval("8 >> 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 4);
}

// =========================================================================
//  Comparison
// =========================================================================

TEST(OperatorTest, LessThan)
{
    rel::Value v = Eval("1 < 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, LessThanFalse)
{
    rel::Value v = Eval("3 < 2");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, GreaterThan)
{
    rel::Value v = Eval("3 > 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, LessEqual)
{
    rel::Value v = Eval("2 <= 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, GreaterEqual)
{
    rel::Value v = Eval("2 >= 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, Equal)
{
    rel::Value v = Eval("1 == 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, EqualFalse)
{
    rel::Value v = Eval("1 == 2");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, NotEqual)
{
    rel::Value v = Eval("1 != 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, KeywordEquals)
{
    rel::Value v = Eval("1 EQUALS 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, KeywordNotEquals)
{
    rel::Value v = Eval("1 NOTEQUALS 2");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

// =========================================================================
//  Bitwise
// =========================================================================

TEST(OperatorTest, BitwiseAnd)
{
    rel::Value v = Eval("5 & 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, BitwiseOr)
{
    rel::Value v = Eval("5 | 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 7);
}

TEST(OperatorTest, BitwiseXor)
{
    rel::Value v = Eval("5 ^ 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 6);
}

// =========================================================================
//  Unary
// =========================================================================

TEST(OperatorTest, UnaryNegate)
{
    rel::Value v = Eval("-3.14");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), -3.14);
}

TEST(OperatorTest, UnaryLogicalNotTrue)
{
    rel::Value v = Eval("!1");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, UnaryLogicalNotFalse)
{
    rel::Value v = Eval("!0");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, KeywordNot)
{
    rel::Value v = Eval("NOT 1");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, UnaryBitwiseNot)
{
    rel::Value v = Eval("~5");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), ~5);
}

// =========================================================================
//  Logical (short-circuit)
// =========================================================================

TEST(OperatorTest, AndTrue)
{
    rel::Value v = Eval("1 && 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, AndFalse)
{
    rel::Value v = Eval("1 && 0");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, OrTrue)
{
    rel::Value v = Eval("0 || 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, OrFalse)
{
    rel::Value v = Eval("0 || 0");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, KeywordAnd)
{
    rel::Value v = Eval("1 AND 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, KeywordOr)
{
    rel::Value v = Eval("0 OR 1");
    EXPECT_TRUE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, ShortCircuitAnd)
{
    // Smoke test: 0 && any-value doesn't crash (short-circuit).
    rel::Value v = Eval("0 && 1");
    EXPECT_FALSE(v.as_measurement().as_scalar<bool>());
}

TEST(OperatorTest, ComplexExpression)
{
    rel::Value v = Eval("1 + 2 * 3 - 4 / 2");
    EXPECT_EQ(v.as_measurement().as_scalar<double>(), 5);  // 1+6-2
}

TEST(OperatorTest, NegateExpression)
{
    rel::Value v = Eval("-(1 + 2)");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), -3);
}

// =========================================================================
//  Operator with units
// =========================================================================

TEST(OperatorTest, AddWithUnit)
{
    rel::Value v = Eval("1GHz + 2GHz");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_TRUE(v.as_measurement().unit().has_dimension());
}

TEST(OperatorTest, MulWithUnit)
{
    rel::Value v = Eval("2 * 3MHz");
    EXPECT_TRUE(v.is_measurement());
}

// =========================================================================
//  Operator with units — display of compound (decomposed) units
// =========================================================================

TEST(OperatorTest, UnitDisplayAW)
{
    // A * W should display as "A*W", not bare SI exponents.
    rel::Value v = Eval("1 A * 1 W");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_EQ(us, "A*W");
}

TEST(OperatorTest, UnitDisplayVPerA)
{
    // V / A  →  "Ohm"
    rel::Value v = Eval("1 V / 1 A");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_EQ(us, "Ohm");
}

TEST(OperatorTest, UnitDisplayMeterPerSec)
{
    // meter / sec  →  "meter/sec"
    rel::Value v = Eval("1 meter / 1 sec");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_EQ(us, "meter/sec");
}

TEST(OperatorTest, UnitDisplayOhmTimesA)
{
    // Ohm * A  →  "V"
    rel::Value v = Eval("1 Ohm * 1 A");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_EQ(us, "V");
}

TEST(OperatorTest, UnitDisplayWS)
{
    // W * sec  →  "J"
    rel::Value v = Eval("1 W * 1 sec");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_EQ(us, "J");
}

TEST(OperatorTest, UnitDisplayHzSecDimless)
{
    // Hz * sec  →  dimensionless
    rel::Value v = Eval("1 Hz * 1 sec");
    EXPECT_TRUE(v.is_measurement());
    std::string us = v.as_measurement().unit().to_string();
    EXPECT_TRUE(us.empty());
}

TEST(OperatorTest, UnitDisplayCompoundRoundTrip)
{
    // Compound unit expressions evaluate correctly and display as expected.
    auto check = [](const char* expr, const char* expected_unit_str) {
        rel::Value v = Eval(expr);
        EXPECT_TRUE(v.is_measurement()) << expr;
        std::string us = v.as_measurement().unit().to_string();
        EXPECT_EQ(us, expected_unit_str) << expr;
    };
    check("1 A * 1 W",   "A*W");
    check("1 W * 1 sec", "J");     // Joule
    check("1 V / 1 A",   "Ohm");
    check("1 Ohm * 1 A", "V");     // Ohm * A = V
    check("1 meter / 1 sec", "meter/sec");
}

// =========================================================================
//  Conditional (?:) and If
// =========================================================================

TEST(OperatorTest, ConditionalTrue)
{
    rel::Value v = Eval("TRUE ? 10 : 20");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 10);
}

TEST(OperatorTest, ConditionalFalse)
{
    rel::Value v = Eval("FALSE ? 10 : 20");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 20);
}

TEST(OperatorTest, ConditionalWithComparison)
{
    rel::Value v = Eval("(1 < 2) ? 100 : 200");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 100);
}

TEST(OperatorTest, ConditionalVectorBroadcast)
{
    rel::Value v = Eval("{1, 2, 3} ? 10 : 20");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
}

TEST(OperatorTest, IfSimple)
{
    rel::Value v = Eval("if(TRUE) then 42 else 0");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 42);
}

TEST(OperatorTest, IfElseifTrue)
{
    rel::Value v = Eval("if(FALSE) then 1 elseif(TRUE) then 2 else 3");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 2);
}

TEST(OperatorTest, IfElseDefault)
{
    rel::Value v = Eval("if(FALSE) then 1 elseif(FALSE) then 2 else 3");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 3);
}

// --- broadcast: condition vector + scalar branches ---

TEST(OperatorTest, ConditionalCondVectorThenScalarElseScalar)
{
    // {TRUE, FALSE, TRUE} ? 10 : 20  →  [10, 20, 10]
    rel::Value v = Eval("{TRUE, FALSE, TRUE} ? 10 : 20");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 10);
}

TEST(OperatorTest, ConditionalCondScalarThenVectorElseScalar)
{
    // TRUE ? {10, 20, 30} : 0  →  [10, 20, 30]
    rel::Value v = Eval("TRUE ? {10, 20, 30} : 0");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
}

TEST(OperatorTest, ConditionalCondVectorThenVectorElseVector)
{
    // {TRUE, FALSE, TRUE} ? {1, 2, 3} : {10, 20, 30}  →  [1, 20, 3]
    rel::Value v = Eval("{TRUE, FALSE, TRUE} ? {1, 2, 3} : {10, 20, 30}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 3);
}

// --- if with broadcast ---

TEST(OperatorTest, IfCondVector)
{
    // if({TRUE, FALSE}) then {10, 20} else {100, 200}  →  [10, 200]
    rel::Value v = Eval("if({TRUE, FALSE}) then {10, 20} else {100, 200}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 200);
}

TEST(OperatorTest, IfCondVectorWithElseif)
{
    // if({FALSE,TRUE,FALSE}) then {1,2,3} elseif({TRUE,FALSE,TRUE}) then {10,20,30} else {100,200,300}
    rel::Value v = Eval("if({FALSE,TRUE,FALSE}) then {1,2,3} elseif({TRUE,FALSE,TRUE}) then {10,20,30} else {100,200,300}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 10);   // cond0=F→skip, cond1=T→val1
    EXPECT_EQ(vec[1], 2);    // cond0=T→val0
    EXPECT_EQ(vec[2], 30);   // cond0=F→skip, cond1=T→val1
}

TEST(OperatorTest, IfScalarCondVectorBranchElseScalar)
{
    // if(TRUE) then {10, 20} else 0  →  [10, 20]
    rel::Value v = Eval("if(TRUE) then {10, 20} else 0");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<int>();
    ASSERT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
}
