#include "gtest/gtest.h"
#include <string>

#include "core/equation_value.h"
#include "equation_value_test_utils.h"

using namespace xequation;

TEST(EquationValue, Null)
{
    EquationValue v;
    EXPECT_TRUE(v.IsNull());
    EXPECT_FALSE(v.HasValue());
    EXPECT_EQ(ValueToString(v), "null");
}

TEST(EquationValue, RelValueConstruction)
{
    EquationValue v(rel::Value::Integer(7));
    ASSERT_TRUE(v.HasValue());
    EXPECT_EQ(AsScalar<int>(v), 7);

    // 直接访问底层 rel::Value
    const rel::Value &rv = v.Value();
    EXPECT_EQ(rv.as_measurement().as_scalar<int>(), 7);
}

TEST(EquationValue, ScalarConstructorsMapToRelValue)
{
    EquationValue vi(42);
    EXPECT_TRUE(IsIntegerValue(vi));
    EXPECT_EQ(AsScalar<int>(vi), 42);

    EquationValue vd(3.14);
    EXPECT_TRUE(IsRealValue(vd));
    EXPECT_DOUBLE_EQ(AsScalar<double>(vd), 3.14);

    EquationValue vb(true);
    EXPECT_TRUE(IsBooleanValue(vb));
    EXPECT_EQ(AsScalar<bool>(vb), true);

    EquationValue vs(std::string("hello"));
    EXPECT_TRUE(IsStringValue(vs));
    EXPECT_EQ(AsScalar<std::string>(vs), "hello");
}

TEST(EquationValue, CastThrowsOnNull)
{
    EquationValue v;  // none
    EXPECT_TRUE(v.IsNull());
    EXPECT_FALSE(v.HasValue());
}

TEST(EquationValue, CopySemantics)
{
    EquationValue a(42);
    EquationValue b(a);  // optional 拷贝
    EXPECT_EQ(AsScalar<int>(b), 42);
    b = EquationValue(3.14);
    EXPECT_TRUE(IsRealValue(b));
    EXPECT_DOUBLE_EQ(AsScalar<double>(b), 3.14);
    EXPECT_EQ(AsScalar<int>(a), 42);  // a 不受影响
}

TEST(EquationValue, ToString)
{
    EXPECT_EQ(ValueToString(EquationValue()), "null");
    EXPECT_FALSE(ValueToString(EquationValue(42)).empty());
    EXPECT_FALSE(ValueToString(EquationValue(3.14)).empty());
    EXPECT_FALSE(ValueToString(EquationValue(std::string("x"))).empty());
}
