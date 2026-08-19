#include "gtest/gtest.h"
#include <stdexcept>
#include <string>

#include "core/equation_value.h"

using namespace xequation;

TEST(EquationValue, Null)
{
    EquationValue v;
    EXPECT_TRUE(v.IsNull());
    EXPECT_FALSE(v.IsRelValue());
    EXPECT_FALSE(v.IsPyObject());
    EXPECT_EQ(v.ToString(), "null");

    EXPECT_TRUE(EquationValue::Null().IsNull());
}

TEST(EquationValue, ScalarConstructorsMapToRelValue)
{
    EquationValue vi(42);
    EXPECT_TRUE(vi.IsRelValue());
    EXPECT_TRUE(vi.IsInteger());
    EXPECT_EQ(vi.Cast<int>(), 42);

    EquationValue vd(3.14);
    EXPECT_TRUE(vd.IsReal());
    EXPECT_DOUBLE_EQ(vd.Cast<double>(), 3.14);

    EquationValue vb(true);
    EXPECT_TRUE(vb.IsBoolean());
    EXPECT_EQ(vb.Cast<bool>(), true);

    EquationValue vs(std::string("hello"));
    EXPECT_TRUE(vs.IsString());
    EXPECT_EQ(vs.Cast<std::string>(), "hello");
}

TEST(EquationValue, RelValueConstruction)
{
    rel::Value rv = rel::Value::Integer(7);
    EquationValue v(rv);
    EXPECT_TRUE(v.IsRelValue());
    EXPECT_FALSE(v.IsNull());
    EXPECT_EQ(v.Cast<int>(), 7);

    // 直接提取 rel::Value
    rel::Value extracted = v.Cast<rel::Value>();
    EXPECT_EQ(extracted.as_measurement().as_scalar<int>(), 7);
}

TEST(EquationValue, CastThrowsOnNull)
{
    EquationValue v;
    EXPECT_THROW(v.Cast<int>(), std::runtime_error);
}

TEST(EquationValue, CopySemantics)
{
    EquationValue a(42);
    EquationValue b(a);  // 拷贝：variant 内联拷贝，无堆分配
    EXPECT_EQ(b.Cast<int>(), 42);
    b = EquationValue(3.14);  // 赋值替换
    EXPECT_TRUE(b.IsReal());
    EXPECT_DOUBLE_EQ(b.Cast<double>(), 3.14);
    EXPECT_EQ(a.Cast<int>(), 42);  // a 不受影响
}

TEST(EquationValue, ToString)
{
    EXPECT_EQ(EquationValue().ToString(), "null");
    // rel::Value 标量走 Format()，输出非空
    EXPECT_FALSE(EquationValue(42).ToString().empty());
    EXPECT_FALSE(EquationValue(3.14).ToString().empty());
    EXPECT_FALSE(EquationValue(std::string("x")).ToString().empty());
}
