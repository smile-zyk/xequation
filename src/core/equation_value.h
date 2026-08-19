#pragma once
#include <boost/blank.hpp>
#include <boost/variant.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "value.h"  // rel::Value（来自 REL rel_runtime）
#include "py_object_ref.h"

namespace xequation
{

// 架构数据交换协议：一个 EquationValue 恰好是下面三种状态之一。
//   null        —— boost::blank，无值
//   rel::Value  —— REL 值（标量/向量/矩阵/DataArray，含 unit）
//   PyObjectRef —— 不透明 Python 对象（list/dict/自定义类/callable…）
//
// 相比旧版类型擦除（unique_ptr<ValueBase> + 每值一次堆分配），
// variant 将 payload 内联存储，零堆分配；类型由 which() 确定，
// 不依赖 typeid / dynamic_cast，跨 DLL（rel_runtime 是 SHARED）安全。
class EquationValue
{
  public:
    enum class Kind
    {
        kNull,
        kRelValue,
        kPyObject
    };

    // ---- 构造 ---------------------------------------------------------

    EquationValue() noexcept : storage_(boost::blank()) {}
    EquationValue(boost::blank) noexcept : storage_(boost::blank()) {}

    // rel::Value
    EquationValue(const rel::Value &v) : storage_(v) {}
    EquationValue(rel::Value &&v) : storage_(std::move(v)) {}

    // 不透明 Python 对象
    EquationValue(const PyObjectRef &p) : storage_(p) {}
    EquationValue(PyObjectRef &&p) : storage_(std::move(p)) {}

    // 便捷标量构造（映射到 rel::Value）
    EquationValue(int v) : storage_(rel::Value::Integer(v)) {}
    EquationValue(double v) : storage_(rel::Value::Real(v)) {}
    EquationValue(bool v) : storage_(rel::Value::Boolean(v)) {}
    EquationValue(const char *v) : storage_(rel::Value::String(std::string(v))) {}
    EquationValue(const std::string &v) : storage_(rel::Value::String(v)) {}

    // ---- 类型查询 -----------------------------------------------------

    static const EquationValue &Null()
    {
        static const EquationValue nullValue;
        return nullValue;
    }

    bool IsNull() const noexcept
    {
        return storage_.which() == 0;
    }
    bool IsRelValue() const noexcept
    {
        return storage_.which() == 1;
    }
    bool IsPyObject() const noexcept
    {
        return storage_.which() == 2;
    }
    Kind kind() const noexcept;

    // rel::Value 便捷类型查询（仅当 IsRelValue() 且为 Measurement 标量时有意义）
    bool IsInteger() const;
    bool IsReal() const;
    bool IsBoolean() const;
    bool IsString() const;

    // ---- 访问 ---------------------------------------------------------

    const rel::Value &AsRel() const
    {
        return boost::get<rel::Value>(storage_);
    }
    rel::Value &AsRel()
    {
        return boost::get<rel::Value>(storage_);
    }
    const PyObjectRef &AsPyObject() const
    {
        return boost::get<PyObjectRef>(storage_);
    }
    PyObjectRef &AsPyObject()
    {
        return boost::get<PyObjectRef>(storage_);
    }

    template <typename T>
    typename std::decay<T>::type Cast() const
    {
        static_assert(
            !std::is_same<typename std::decay<T>::type, EquationValue>::value,
            "Cannot cast EquationValue to EquationValue"
        );

        if (IsNull())
        {
            throw std::runtime_error("Cannot cast null EquationValue");
        }
        typedef typename std::decay<T>::type D;
        return CastImpl<D>(std::is_same<D, rel::Value>(), std::is_same<D, PyObjectRef>());
    }

    // ---- 显示 ---------------------------------------------------------

    std::string ToString() const;

  private:
    template <typename D>
    D CastImpl(std::true_type /*is_rel*/, std::false_type) const
    {
        return AsRel();
    }

    template <typename D>
    D CastImpl(std::false_type, std::true_type /*is_py*/) const
    {
        return AsPyObject();
    }

    template <typename D>
    D CastImpl(std::false_type, std::false_type) const
    {
        // 标量提取：从 rel::Value 的 Measurement 取
        const rel::Value &rv = AsRel();
        if (!rv.is_measurement())
        {
            throw std::runtime_error("Cannot cast non-measurement EquationValue to scalar");
        }
        return rv.as_measurement().as_scalar<D>();
    }

    boost::variant<boost::blank, rel::Value, PyObjectRef> storage_;
};

} // namespace xequation
