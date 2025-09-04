#include "gtest/gtest.h"
#include <complex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "value.h"

using namespace xexprengine;

TEST(Value, ComparisonOperators)
{
    Value v1 = 10;
    Value v2 = 10;
    Value v3 = 20;
    Value v_null;

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_TRUE(v1 != v3);
    EXPECT_TRUE(v1 < v3);
    EXPECT_TRUE(v3 > v1);
    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v3 >= v1);

    EXPECT_FALSE(v1 == v_null);
    EXPECT_TRUE(v_null == v_null);
    EXPECT_TRUE(v_null < v1);
}

TEST(Value, MoveConstructorAndAssignment)
{
    Value v1 = "move_test";
    Value v2 = std::move(v1);
    EXPECT_TRUE(v1.IsNull());
    EXPECT_EQ(v2.ToString(), "move_test");

    Value v3;
    v3 = std::move(v2);
    EXPECT_TRUE(v2.IsNull());
    EXPECT_EQ(v3.ToString(), "move_test");
}

TEST(Value, TypeInfo)
{
    Value v_int = 42;
    Value v_double = 3.14;
    Value v_str = std::string("abc");
    EXPECT_EQ(v_int.Type(), typeid(int));
    EXPECT_EQ(v_double.Type(), typeid(double));
    EXPECT_EQ(v_str.Type(), typeid(std::string));
}

TEST(Value, CastThrowsOnWrongType)
{
    Value v = 123;
    EXPECT_THROW(v.Cast<double>(), std::runtime_error);
}

TEST(Value, NestedContainers)
{
    Value v_nested = std::vector<std::map<std::string, int>>{
        {{"a", 1}, {"b", 2}},
        {{"c", 3}}
    };
    EXPECT_EQ(v_nested.ToString(), "[{a: 1, b: 2}, {c: 3}]");
}

TEST(Value, ToStringHelper)
{
    // Null value
    Value null_val;
    EXPECT_EQ(null_val.ToString(), "null");

    // Char value
    Value char_val = 'A';
    EXPECT_EQ(char_val.ToString(), "A");

    // Float value
    Value float_val = 2.5f;
    EXPECT_EQ(float_val.ToString(), "2.500000");

    // Long value
    Value long_val = static_cast<long>(123456789);
    EXPECT_EQ(long_val.ToString(), "123456789");

    // Unsigned int value
    Value uint_val = static_cast<unsigned int>(123);
    EXPECT_EQ(uint_val.ToString(), "123");

    // Vector of strings
    Value vec_str_val = std::vector<std::string>{"a", "b", "c"};
    EXPECT_EQ(vec_str_val.ToString(), "[a, b, c]");

    // List of bools
    Value list_bool_val = std::list<bool>{true, false, true};
    EXPECT_EQ(list_bool_val.ToString(), "[true, false, true]");

    // Map with Value keys and values
    Value map_val2 = std::map<Value, Value>{{"x", 1}, {"y", 2}};
    EXPECT_EQ(map_val2.ToString(), "{x: 1, y: 2}");

    // Set of ints
    Value set_int_val = std::set<int>{10, 20, 30};
    EXPECT_EQ(set_int_val.ToString(), "{10, 20, 30}");

    // Unordered set of strings
    Value uset_str_val = std::unordered_set<std::string>{"foo", "bar"};
    auto uset_str = uset_str_val.ToString();
    EXPECT_TRUE(uset_str == "{foo, bar}" || uset_str == "{bar, foo}");

    // Nested vector
    Value nested_vec_val = std::vector<std::vector<int>>{{1,2},{3,4}};
    EXPECT_EQ(nested_vec_val.ToString(), "[[1, 2], [3, 4]]");

    // Nested map
    Value nested_map_val = std::map<std::string, std::map<std::string, int>>{
        {"outer", {{"inner", 99}}}
    };
    EXPECT_EQ(nested_map_val.ToString(), "{outer: {inner: 99}}");

    // Complex double
    Value complex_double_val = std::complex<double>(2.0, -3.5);
    EXPECT_EQ(complex_double_val.ToString(), "(2.000000 - 3.500000j)");

    // Empty vector
    Value empty_vec_val = std::vector<int>{};
    EXPECT_EQ(empty_vec_val.ToString(), "[]");

    // Empty map
    Value empty_map_val = std::map<std::string, int>{};
    EXPECT_EQ(empty_map_val.ToString(), "{}");

    // Empty set
    Value empty_set_val = std::set<int>{};
    EXPECT_EQ(empty_set_val.ToString(), "{}");
}

TEST(Value, InitializationAndNullCheck) 
{
    Value null_val;
    EXPECT_TRUE(null_val.IsNull());

    Value int_val = 42;
    EXPECT_FALSE(int_val.IsNull());
    EXPECT_EQ(int_val.Cast<int>(), 42);

    Value double_val = 2.71828;
    EXPECT_FALSE(double_val.IsNull());
    EXPECT_EQ(double_val.Cast<double>(), 2.71828);

    Value str_val = "Hello";
    EXPECT_FALSE(str_val.IsNull());
    EXPECT_EQ(str_val.Cast<std::string>(), "Hello");

    Value vec_val = std::vector<int>{1, 2, 3};
    EXPECT_FALSE(vec_val.IsNull());
    EXPECT_EQ(vec_val.Cast<std::vector<int>>(), std::vector<int>({1, 2, 3}));
    
    Value list_val = std::list<double>{1.1, 2.2, 3.3};
    EXPECT_FALSE(list_val.IsNull());
    EXPECT_EQ(list_val.Cast<std::list<double>>(), std::list<double>({1.1, 2.2, 3.3}));

    Value map_val = std::map<std::string, int>{{"one", 1}, {"two", 2}};
    EXPECT_FALSE(map_val.IsNull());
    auto map_value = map_val.Cast<std::map<std::string, int>>();
    EXPECT_EQ(map_value["one"], 1);
    EXPECT_EQ(map_value["two"], 2);

    Value set_val = std::set<std::string>{"apple", "banana"};
    EXPECT_FALSE(set_val.IsNull());
    EXPECT_EQ(set_val.Cast<std::set<std::string>>(), std::set<std::string>({"apple", "banana"}));

    Value bool_true_val = true;
    EXPECT_FALSE(bool_true_val.IsNull());
    EXPECT_EQ(bool_true_val.Cast<bool>(), true);

    Value bool_false_val = false;
    EXPECT_FALSE(bool_false_val.IsNull());
    EXPECT_EQ(bool_false_val.Cast<bool>(), false);
}

TEST(Value, ContainerOfValues) 
{
    Value vec_val = std::vector<Value>{1, 2.5, "test", true, false};
    EXPECT_FALSE(vec_val.IsNull());
    auto vec = vec_val.Cast<std::vector<Value>>();
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0].Cast<int>(), 1);
    EXPECT_EQ(vec[1].Cast<double>(), 2.5);
    EXPECT_EQ(vec[2].Cast<std::string>(), "test");
    EXPECT_EQ(vec[3].Cast<bool>(), true);
    EXPECT_EQ(vec[4].Cast<bool>(), false);

    Value map_val = std::map<Value, Value>{{"key1", 100}, {5, "value"}, {true, false}};
    EXPECT_FALSE(map_val.IsNull());
    auto map = map_val.Cast<std::map<Value, Value>>();
    EXPECT_EQ(map["key1"].Cast<int>(), 100);
    EXPECT_EQ(map[5].Cast<std::string>(), "value");
    EXPECT_EQ(map[true].Cast<bool>(), false);
    
    Value set_val = std::set<Value>{1, 2, 3, true, false};
    EXPECT_FALSE(set_val.IsNull());
    auto set = set_val.Cast<std::set<Value>>();
    EXPECT_EQ(set.size(), 5);
    EXPECT_TRUE(set.find(1) != set.end()); 
    EXPECT_TRUE(set.find(2) != set.end());
    EXPECT_TRUE(set.find(3) != set.end());
    EXPECT_TRUE(set.find(true) != set.end());
    EXPECT_TRUE(set.find(false) != set.end());

    Value uset_val = std::unordered_set<Value>{1, 2, 3, true, false};
    EXPECT_FALSE(uset_val.IsNull());
    auto uset = uset_val.Cast<std::unordered_set<Value>>();
    EXPECT_EQ(uset.size(), 5);
    EXPECT_TRUE(uset.find(1) != uset.end());
    EXPECT_TRUE(uset.find(2) != uset.end());
    EXPECT_TRUE(uset.find(3) != uset.end());
    EXPECT_TRUE(uset.find(true) != uset.end());
    EXPECT_TRUE(uset.find(false) != uset.end());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}