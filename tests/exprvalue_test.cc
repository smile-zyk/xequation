#include "gtest/gtest.h"
#include <string>
#include <vector>

#include "exprvalue.h"

TEST(ExprValue, InitializationAndNullCheck) 
{
    ExprValue ev1;
    EXPECT_TRUE(ev1.IsNull());

    ExprValue ev2 = 42;
    EXPECT_FALSE(ev2.IsNull());
    EXPECT_EQ(ev2.Cast<int>(), 42);

    ExprValue ev3 = 2.71828;
    EXPECT_FALSE(ev3.IsNull());
    EXPECT_EQ(ev3.Cast<double>(), 2.71828);

    ExprValue ev4 = "Hello";
    EXPECT_FALSE(ev4.IsNull());
    EXPECT_EQ(ev4.Cast<std::string>(), "Hello");

    ExprValue ev5 = std::vector<int>{1, 2, 3};
    EXPECT_FALSE(ev5.IsNull());
    EXPECT_EQ(ev5.Cast<std::vector<int>>(), std::vector<int>({1, 2, 3}));
    
    ExprValue ev6 = std::list<double>{1.1, 2.2, 3.3};
    EXPECT_FALSE(ev6.IsNull());
    EXPECT_EQ(ev6.Cast<std::list<double>>(), std::list<double>({1.1, 2.2, 3.3}));

    ExprValue ev7 = std::map<std::string, int>{{"one", 1}, {"two", 2}};
    EXPECT_FALSE(ev7.IsNull());
    auto map_value = ev7.Cast<std::map<std::string, int>>();
    EXPECT_EQ(map_value["one"], 1);
    EXPECT_EQ(map_value["two"], 2);

    ExprValue ev8 = std::set<std::string>{"apple", "banana"};
    EXPECT_FALSE(ev8.IsNull());
    EXPECT_EQ(ev8.Cast<std::set<std::string>>(), std::set<std::string>({"apple", "banana"}));
}

TEST(ExprValue, NestedTypes) 
{
    ExprValue ev1 = std::vector<ExprValue>{ExprValue(1), ExprValue(2.5), ExprValue("test")};
    EXPECT_FALSE(ev1.IsNull());
    auto vec_value = ev1.Cast<std::vector<ExprValue>>();
    EXPECT_EQ(vec_value.size(), 3);
    EXPECT_EQ(vec_value[0].Cast<int>(), 1);
    EXPECT_EQ(vec_value[1].Cast<double>(), 2.5);
    EXPECT_EQ(vec_value[2].Cast<std::string>(), "test");
    EXPECT_STREQ(ConvertToString(vec_value).c_str(), "[1, 2.500000, test]");

    ExprValue ev2 = std::map<std::string, ExprValue>{{"key1", ExprValue(100)}, {"key2", ExprValue("value")}};
    EXPECT_FALSE(ev2.IsNull());
    auto map_value = ev2.Cast<std::map<std::string, ExprValue>>();
    EXPECT_EQ(map_value["key1"].Cast<int>(), 100);
    EXPECT_EQ(map_value["key2"].Cast<std::string>(), "value");
    EXPECT_STREQ(ConvertToString(map_value).c_str(), "{key1: 100, key2: value}");

    ExprValue ev3 = std::set<ExprValue>{ExprValue(1), ExprValue(2), ExprValue(3)};
    EXPECT_FALSE(ev3.IsNull());
    auto set_value = ev3.Cast<std::set<ExprValue>>();
    EXPECT_EQ(set_value.size(), 3);
    EXPECT_TRUE(set_value.find(ExprValue(1)) != set_value.end());
    EXPECT_TRUE(set_value.find(ExprValue(2)) != set_value.end());
    EXPECT_TRUE(set_value.find(ExprValue(3)) != set_value.end());
    EXPECT_STREQ(ConvertToString(set_value).c_str(), "{1, 2, 3}");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}