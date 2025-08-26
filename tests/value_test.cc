#include "gtest/gtest.h"
#include <string>
#include <vector> 

#include "value.hpp"

using namespace xexprengine;

struct B{};

std::string ConvertToString(const B &value)
{
    return "struct B";
}

TEST(ValueHelper, ToString)
{
    // test int
    int int_ = 1;
    std::string int_str = ValueHelper::ToString(int_);
    std::string int_expect_str = "1";
    EXPECT_STREQ(int_str.c_str(), int_expect_str.c_str());

    // test float
    float float_ = 3.f;
    std::string float_str = ValueHelper::ToString(float_);
    std::string float_expect_str = "3.000000";
    EXPECT_STREQ(float_str.c_str(), float_expect_str.c_str());

    // test double
    double double_ = 3.1415926;
    std::string double_str = ValueHelper::ToString(double_);
    std::string double_expect_str = "3.141593";
    EXPECT_STREQ(double_str.c_str(), double_expect_str.c_str());

    // test string
    std::string string_ = "test_string";
    std::string string_str = ValueHelper::ToString(string_);
    EXPECT_STREQ(string_str.c_str(), string_.c_str());

    // test custom type with ToString method
    struct A
    {
        std::string ToString() const { return "struct A"; }
    };

    // test custom type with ToString method
    A custom_to_string_;
    std::string custom_str = ValueHelper::ToString(custom_to_string_);
    std::string custom_expect_str = "struct A";
    EXPECT_STREQ(custom_str.c_str(), custom_expect_str.c_str());

    // test custom type with ConvertToString function
    B custom_convert_to_string_;
    std::string convert_str = ValueHelper::ToString(custom_convert_to_string_);
    std::string convert_expect_str = "struct B";
    EXPECT_STREQ(convert_str.c_str(), convert_expect_str.c_str());

    // test vector
    std::vector<int> vector_ = {1, 2, 3};
    std::string vec_str = ValueHelper::ToString(vector_);
    std::string vec_expect_str = "[1, 2, 3]";
    EXPECT_STREQ(vec_str.c_str(), vec_expect_str.c_str());

    // test list
    std::list<std::string> list_ = {"one", "two", "three"};
    std::string list_str = ValueHelper::ToString(list_);
    std::string list_expect_str = "[one, two, three]";
    EXPECT_STREQ(list_str.c_str(), list_expect_str.c_str());

    // test map
    std::map<std::string, int> map_ = {{"one", 1}, {"two", 2}}; 
    std::string map_str = ValueHelper::ToString(map_);
    std::string map_expect_str = "{one: 1, two: 2}";
    EXPECT_STREQ(map_str.c_str(), map_expect_str.c_str());

    // test set
    std::set<int> set_ = {1, 2, 3};
    std::string set_str = ValueHelper::ToString(set_);
    std::string set_expect_str = "{1, 2, 3}";
    EXPECT_STREQ(set_str.c_str(), set_expect_str.c_str());
}

TEST(Value, InitializationAndNullCheck)
{
    NullValue null_value;
    EXPECT_TRUE(null_value.IsNull());
    EXPECT_STREQ(null_value.ToString().c_str(), "null");
    EXPECT_EQ(null_value.Type(), typeid(void));

    Value<int> int_value(42);
    EXPECT_FALSE(int_value.IsNull());
    EXPECT_EQ(int_value.value(), 42);
    EXPECT_STREQ(int_value.ToString().c_str(), "42");
    EXPECT_EQ(int_value.Type(), typeid(int));

    Value<double> double_value(3.14);
    EXPECT_FALSE(double_value.IsNull());
    EXPECT_EQ(double_value.value(), 3.14);
    EXPECT_STREQ(double_value.ToString().c_str(), "3.140000");
    EXPECT_EQ(double_value.Type(), typeid(double));

    Value<std::string> string_value("Hello");
    EXPECT_FALSE(string_value.IsNull());
    EXPECT_EQ(string_value.value(), "Hello");
    EXPECT_STREQ(string_value.ToString().c_str(), "Hello");
    EXPECT_EQ(string_value.Type(), typeid(std::string));

    Value<std::vector<int>> vector_value({1, 2, 3});
    EXPECT_FALSE(vector_value.IsNull());
    EXPECT_EQ(vector_value.value(), std::vector<int>({1, 2, 3}));
    EXPECT_STREQ(vector_value.ToString().c_str(), "[1, 2, 3]");
    EXPECT_EQ(vector_value.Type(), typeid(std::vector<int>));

    Value<std::list<std::string>> list_value({"one", "two", "three"});
    EXPECT_FALSE(list_value.IsNull());
    EXPECT_EQ(list_value.value(), std::list<std::string>({"one", "two", "three"}));
    EXPECT_STREQ(list_value.ToString().c_str(), "[one, two, three]");
    EXPECT_EQ(list_value.Type(), typeid(std::list<std::string>));

    Value<std::map<std::string, int>> map_value({{"one", 1}, {"two", 2}});
    EXPECT_FALSE(map_value.IsNull());
    EXPECT_EQ(map_value.value(), (std::map<std::string, int>{{"one", 1}, {"two", 2}}));
    EXPECT_STREQ(map_value.ToString().c_str(), "{one: 1, two: 2}");
    EXPECT_EQ(map_value.Type(), typeid(std::map<std::string, int>));

    Value<std::set<int>> set_value({1, 2, 3});
    EXPECT_FALSE(set_value.IsNull());
    EXPECT_EQ(set_value.value(), (std::set<int>{1, 2, 3}));
    EXPECT_STREQ(set_value.ToString().c_str(), "{1, 2, 3}");
    EXPECT_EQ(set_value.Type(), typeid(std::set<int>));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}