#include <gtest/gtest.h>
#include <memory>
#include <pybind11/cast.h>
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>
#include "python/value_pybind_converter.h"
#include "core/value.h"

using namespace xequation;

class PyObjectConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        pybind11::initialize_interpreter();
    }

    void TearDown() override {
        pybind11::finalize_interpreter();
    }
};

TEST_F(PyObjectConverterTest, ConvertInt) {
    Value value(42);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::int_>(obj));
    EXPECT_EQ(obj.cast<int>(), 42);
}

TEST_F(PyObjectConverterTest, ConvertDouble) {
    Value value(3.14);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::float_>(obj));
    EXPECT_DOUBLE_EQ(obj.cast<double>(), 3.14);
}

TEST_F(PyObjectConverterTest, ConvertString) {
    Value value("hello world");
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::str>(obj));
    EXPECT_EQ(obj.cast<std::string>(), "hello world");
}

TEST_F(PyObjectConverterTest, ConvertBool) {
    Value value(true);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::bool_>(obj));
    EXPECT_EQ(obj.cast<bool>(), true);
}

TEST_F(PyObjectConverterTest, ConvertNull) {
    Value value;
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(obj.is_none());
}

TEST_F(PyObjectConverterTest, ConvertVectorInt) {
    std::vector<int> vec{1, 2, 3, 4, 5};
    Value value(vec);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::list>(obj));
    
    pybind11::list list = obj.cast<pybind11::list>();
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list[0].cast<int>(), 1);
    EXPECT_EQ(list[1].cast<int>(), 2);
    EXPECT_EQ(list[2].cast<int>(), 3);
    EXPECT_EQ(list[3].cast<int>(), 4);
    EXPECT_EQ(list[4].cast<int>(), 5);
}

TEST_F(PyObjectConverterTest, ConvertVectorDouble) {
    std::vector<double> vec{1.1, 2.2, 3.3};
    Value value(vec);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::list>(obj));
    
    pybind11::list list = obj.cast<pybind11::list>();
    EXPECT_EQ(list.size(), 3);
    EXPECT_DOUBLE_EQ(list[0].cast<double>(), 1.1);
    EXPECT_DOUBLE_EQ(list[1].cast<double>(), 2.2);
    EXPECT_DOUBLE_EQ(list[2].cast<double>(), 3.3);
}

TEST_F(PyObjectConverterTest, ConvertVectorString) {
    std::vector<std::string> vec{"a", "bb", "ccc"};
    Value value(vec);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::list>(obj));
    
    pybind11::list list = obj.cast<pybind11::list>();
    EXPECT_EQ(list.size(), 3);
    EXPECT_EQ(list[0].cast<std::string>(), "a");
    EXPECT_EQ(list[1].cast<std::string>(), "bb");
    EXPECT_EQ(list[2].cast<std::string>(), "ccc");
}

TEST_F(PyObjectConverterTest, ConvertMapStringString) {
    std::map<std::string, std::string> map{{"key1", "value1"}, {"key2", "value2"}};
    Value value(map);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::dict>(obj));
    
    pybind11::dict dict = obj.cast<pybind11::dict>();
    EXPECT_EQ(dict.size(), 2);
    EXPECT_EQ(dict["key1"].cast<std::string>(), "value1");
    EXPECT_EQ(dict["key2"].cast<std::string>(), "value2");
}

TEST_F(PyObjectConverterTest, ConvertUnorderedMapStringString) {
    std::unordered_map<std::string, std::string> map{{"key1", "value1"}, {"key2", "value2"}};
    Value value(map);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::dict>(obj));
    
    pybind11::dict dict = obj.cast<pybind11::dict>();
    EXPECT_EQ(dict.size(), 2);
    EXPECT_EQ(dict["key1"].cast<std::string>(), "value1");
    EXPECT_EQ(dict["key2"].cast<std::string>(), "value2");
}

TEST_F(PyObjectConverterTest, ConvertLong) {
    Value value(123456789L);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::int_>(obj));
    EXPECT_EQ(obj.cast<long>(), 123456789L);
}

TEST_F(PyObjectConverterTest, ConvertUnsignedInt) {
    Value value(4294967295U);
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::int_>(obj));
    EXPECT_EQ(obj.cast<unsigned int>(), 4294967295U);
}

TEST_F(PyObjectConverterTest, RoundTripConversion) {
    std::vector<int> original{1, 2, 3};
    Value value(original);
    
    pybind11::object obj = pybind11::cast(value);
    
    std::vector<int> result = obj.cast<std::vector<int>>();
    
    EXPECT_EQ(result, original);
}

TEST_F(PyObjectConverterTest, CustomConverterRegistration) {
    struct TestCustomConverter : public value_convert::PyObjectConverter::TypeConverter {
        bool CanConvert(const Value &value) const override {
            return value.Type() == typeid(std::pair<int, int>);
        }
        
        pybind11::object Convert(const Value &value) const override {
            auto pair = value.Cast<std::pair<int, int>>();
            return pybind11::make_tuple(pair.first, pair.second);
        }
    };
    
    value_convert::PyObjectConverter::RegisterConverter(std::unique_ptr<TestCustomConverter>(new TestCustomConverter()));

    std::pair<int, int> test_pair{10, 20};
    Value value(test_pair);
    EXPECT_EQ(value.ToString(), "(10, 20)");
    pybind11::object obj = pybind11::cast(value);
    
    EXPECT_TRUE(pybind11::isinstance<pybind11::tuple>(obj));
    
    pybind11::tuple tuple = obj.cast<pybind11::tuple>();
    EXPECT_EQ(tuple.size(), 2);
    EXPECT_EQ(tuple[0].cast<int>(), 10);
    EXPECT_EQ(tuple[1].cast<int>(), 20);
}

TEST_F(PyObjectConverterTest, PyObjectValue)
{
    pybind11::list m_list;
    m_list.append("1");
    m_list.append(2);
    
    Value package_obj_value = m_list;
    pybind11::object unpackage_obj = pybind11::cast(package_obj_value);
    EXPECT_EQ(m_list.ptr(), unpackage_obj.ptr());

    EXPECT_EQ(package_obj_value.ToString(), "['1', 2]");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}