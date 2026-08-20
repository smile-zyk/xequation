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
// C++ 构造的 rel::Value 载荷经 REL 自己的 Python 绑定呈现为 rel.Value 对象
// （带完整运算符 / is_* 查询 / data_type / data_kind / buffer 协议）。
// 注意：rel.Value.__str__ 输出的是 DataFrame 风格的终端表格（Format），
// 不适合做文本断言，这里改用结构化 API（data_type/data_kind/as_measurement）。

TEST(EquationValueCast, CastInt)
{
    EquationValue value(42);  // -> rel::Value::Integer
    pybind11::object obj = pybind11::cast(value);

    // rel.Value 绑定对象（而非 Python int）
    pybind11::object rel_value_cls = pybind11::module_::import("rel").attr("Value");
    EXPECT_TRUE(pybind11::isinstance(obj, rel_value_cls));
    EXPECT_TRUE(pybind11::cast<bool>(obj.attr("is_measurement")()));
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_type")), "integer");
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_kind")), "scalar");
}

TEST(EquationValueCast, CastDouble)
{
    EquationValue value(3.14);  // -> rel::Value::Real
    pybind11::object obj = pybind11::cast(value);

    pybind11::object rel_value_cls = pybind11::module_::import("rel").attr("Value");
    EXPECT_TRUE(pybind11::isinstance(obj, rel_value_cls));
    EXPECT_TRUE(pybind11::cast<bool>(obj.attr("is_measurement")()));
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_type")), "real");
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_kind")), "scalar");
}

TEST(EquationValueCast, CastString)
{
    EquationValue value(std::string("hello world"));  // -> rel::Value::String
    pybind11::object obj = pybind11::cast(value);

    pybind11::object rel_value_cls = pybind11::module_::import("rel").attr("Value");
    EXPECT_TRUE(pybind11::isinstance(obj, rel_value_cls));
    EXPECT_TRUE(pybind11::cast<bool>(obj.attr("is_measurement")()));
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_type")), "string");
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_kind")), "scalar");
}

TEST(EquationValueCast, CastBool)
{
    EquationValue value(true);  // -> rel::Value::Boolean
    pybind11::object obj = pybind11::cast(value);

    pybind11::object rel_value_cls = pybind11::module_::import("rel").attr("Value");
    EXPECT_TRUE(pybind11::isinstance(obj, rel_value_cls));
    EXPECT_TRUE(pybind11::cast<bool>(obj.attr("is_measurement")()));
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_type")), "boolean");
    EXPECT_EQ(pybind11::cast<std::string>(obj.attr("data_kind")), "scalar");
}

TEST(EquationValueCast, CastNull)
{
    EquationValue value;  // null
    pybind11::object obj = pybind11::cast(value);

    EXPECT_TRUE(obj.is_none());
}

// ---- py::object -> EquationValue（原样保留 PyObjectRef，往返保真）-----

TEST(EquationValueCast, LoadInt)
{
    pybind11::object obj = pybind11::cast(42);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsPyObject());
    EXPECT_FALSE(value.IsRelValue());
    // 透传回同一个 Python 对象，仍可直接 cast<int>
    EXPECT_EQ(pybind11::cast<int>(pybind11::cast(value)), 42);
}

TEST(EquationValueCast, LoadDouble)
{
    pybind11::object obj = pybind11::cast(2.5);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsPyObject());
    EXPECT_DOUBLE_EQ(pybind11::cast<double>(pybind11::cast(value)), 2.5);
}

TEST(EquationValueCast, LoadString)
{
    pybind11::object obj = pybind11::cast("hello");
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsPyObject());
    EXPECT_EQ(pybind11::cast<std::string>(pybind11::cast(value)), "hello");
}

TEST(EquationValueCast, LoadBool)
{
    pybind11::object obj = pybind11::cast(true);
    EquationValue value = pybind11::cast<EquationValue>(obj);

    EXPECT_TRUE(value.IsPyObject());
    EXPECT_EQ(pybind11::cast<bool>(pybind11::cast(value)), true);
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
