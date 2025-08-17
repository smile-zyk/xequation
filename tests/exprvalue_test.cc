#include "gtest/gtest.h"
#include <string>

#include "exprvalue.h"

TEST(Value, ConvertToString)
{
    int a = 1;
    std::string int_str = ConvertToString(a);
    std::string int_expect_str = "1";
    EXPECT_STREQ(int_str.c_str(), int_expect_str.c_str());

    float b = 3.f;
    std::string float_str = ConvertToString(b);
    std::string float_expect_str = "3.000000";
    EXPECT_STREQ(float_str.c_str(), float_expect_str.c_str());

    double c = 3.1415926;
    std::string double_str = ConvertToString(c);
    std::string double_expect_str = "3.141593";
    EXPECT_STREQ(double_str.c_str(), double_expect_str.c_str());

    std::string d = "test_string";
    std::string string_str = ConvertToString(d);
    EXPECT_STREQ(string_str.c_str(), d.c_str());

    struct A
    {
        std::string ToString() const { return "struct A"; }
    };

    A e;
    std::string custom_str = ConvertToString(e);
    std::string custom_expect_str = "struct A";
    EXPECT_STREQ(custom_str.c_str(), custom_expect_str.c_str());

    ExprValue f = "test_expr";
    std::string expr_str = ConvertToString(f);
    std::string expr_expect_str = "test_expr";
    EXPECT_STREQ(expr_str.c_str(), expr_expect_str.c_str());

    ExprValue g = ExprValue::Null();
    
}