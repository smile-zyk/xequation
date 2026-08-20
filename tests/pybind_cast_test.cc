#include <gtest/gtest.h>
#include <pybind11/cast.h>
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>
#include <string>
#include <vector>

#include "python/python_equation_engine.h"
#include "core/equation_value.h"

using namespace xequation;
using namespace xequation::python;

// =========================================================================
//  EquationValue <-> Python 转换
// =========================================================================

// ---- EquationValue -> py::object --------------------------------------

TEST(EquationValueCast, CastInt)
{
    EquationValue value(42);  // -> rel::Value::Integer
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(pybind11::isinstance<pybind11::int_>(obj));
    EXPECT_EQ(obj.cast<int>(), 42);
}

TEST(EquationValueCast, CastDouble)
{
    EquationValue value(3.14);  // -> rel::Value::Real
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(pybind11::isinstance<pybind11::float_>(obj));
    EXPECT_DOUBLE_EQ(obj.cast<double>(), 3.14);
}

TEST(EquationValueCast, CastString)
{
    EquationValue value(std::string("hello world"));  // -> rel::Value::String
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(pybind11::isinstance<pybind11::str>(obj));
    EXPECT_EQ(obj.cast<std::string>(), "hello world");
}

TEST(EquationValueCast, CastBool)
{
    EquationValue value(true);  // -> rel::Value::Boolean
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(pybind11::isinstance<pybind11::bool_>(obj));
    EXPECT_EQ(obj.cast<bool>(), true);
}

TEST(EquationValueCast, CastNull)
{
    EquationValue value;  // null
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(obj.is_none());
}

// ---- py::object -> EquationValue（标量归一化为 rel::Value）-------------

TEST(EquationValueCast, LoadInt)
{
    pybind11::object obj = pybind11::cast(42);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsRelValue());
    EXPECT_TRUE(value.IsInteger());
    EXPECT_EQ(value.Cast<int>(), 42);
}

TEST(EquationValueCast, LoadDouble)
{
    pybind11::object obj = pybind11::cast(2.5);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsReal());
    EXPECT_DOUBLE_EQ(value.Cast<double>(), 2.5);
}

TEST(EquationValueCast, LoadString)
{
    pybind11::object obj = pybind11::cast("hello");
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsString());
    EXPECT_EQ(value.Cast<std::string>(), "hello");
}

TEST(EquationValueCast, LoadBool)
{
    pybind11::object obj = pybind11::cast(true);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsBoolean());
    EXPECT_EQ(value.Cast<bool>(), true);
}

// ---- 容器保持不透明 PyObjectRef，双向透传 ------------------------------

TEST(EquationValueCast, ListRoundTrip)
{
    pybind11::list list;
    list.append(1);
    list.append(2);
    list.append(3);

    EquationValue value = pybind11::cast<EquationValue>(list);
    EXPECT_TRUE(value.IsPyObject());
    EXPECT_FALSE(value.IsRelValue());

    // 透传回同一个 Python 对象
    pybind11::object back = pybind11::cast(value);
    EXPECT_TRUE(pybind11::isinstance<pybind11::list>(back));
    EXPECT_EQ(pybind11::len(back), 3);
}

TEST(EquationValueCast, DictRoundTrip)
{
    pybind11::dict dict;
    dict["a"] = 1;

    EquationValue value = pybind11::cast<EquationValue>(dict);
    EXPECT_TRUE(value.IsPyObject());

    pybind11::object back = pybind11::cast(value);
    EXPECT_TRUE(pybind11::isinstance<pybind11::dict>(back));
    EXPECT_EQ(pybind11::cast<int>(back["a"]), 1);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    // 不能用 pybind11::initialize_interpreter()（默认配置找不到 stdlib 会失败）。
    // 用 REL python_manager 的构建期默认配置初始化嵌入解释器，
    // 引擎构造（GetInstance）会幂等复用已初始化的解释器。
    python_manager::PyEnvManager::SetDefaultPyEnvConfig();
    PythonEquationEngine::GetInstance();
    // 引擎初始化后释放了主线程 GIL，测试体内会直接操作 pybind11 对象，
    // 所以需要在主线程重新持有 GIL。
    pybind11::gil_scoped_acquire acquire;
    return RUN_ALL_TESTS();
}
