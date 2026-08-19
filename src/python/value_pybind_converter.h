#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <string>
#include <vector>

#include "core/equation_value.h"

namespace xequation
{
namespace value_convert
{

// ---------------------------------------------------------------------------
// 延迟 DECREF（PyObjectRef 析构时不持有 GIL：入队，等 GIL 边界统一冲刷）
// ---------------------------------------------------------------------------

inline std::vector<PyObject *> &pending_decrefs()
{
    static thread_local std::vector<PyObject *> queue;
    return queue;
}

inline void SafePyDecRef(PyObject *p)
{
    if (!p)
        return;
    if (PyGILState_Check())
    {
        // 当前线程持有 GIL：立即释放
        Py_DECREF(p);
    }
    else
    {
        // 无 GIL：入队，延迟到 GIL 边界
        pending_decrefs().push_back(p);
    }
}

// 必须在持有 GIL 时调用（现有 gil_scoped_acquire 入口之后）
inline void FlushPendingDecrefs()
{
    auto &q = pending_decrefs();
    for (PyObject *p : q)
    {
        Py_DECREF(p);
    }
    q.clear();
}

inline std::string PyObjectToString(PyObject *p)
{
    if (!p)
        return "<null>";
    PyGILState_STATE st = PyGILState_Ensure();
    std::string result;
    try
    {
        pybind11::object s = pybind11::repr(pybind11::handle(p));
        result = s.cast<std::string>();
    }
    catch (const pybind11::error_already_set &)
    {
        result = "<python object>";
    }
    PyGILState_Release(st);
    return result;
}

// python 层初始化后调用一次：把 PyObject 生命周期操作注入 core
inline void InstallPyObjectOps()
{
    PyObjectOps ops;
    ops.incref = [](PyObject *p) { Py_XINCREF(p); };
    ops.decref = SafePyDecRef;
    ops.to_string = PyObjectToString;
    SetPyObjectOps(ops);
}

} // namespace value_convert
} // namespace xequation

namespace PYBIND11_NAMESPACE
{
namespace detail
{

template <>
struct type_caster<xequation::EquationValue>
{
  public:
    PYBIND11_TYPE_CASTER(xequation::EquationValue, _("EquationValue"));

    // py::object -> EquationValue
    // 标量（bool/int/float/str）归一化为 rel::Value；
    // 其余（list/dict/set/tuple/自定义类/函数/模块…）保留为不透明 PyObjectRef。
    bool load(handle src, bool)
    {
        if (!src)
        {
            return false;
        }
        pybind11::object obj = pybind11::reinterpret_borrow<pybind11::object>(src);

        // 顺序：bool 必须先于 int
        if (pybind11::isinstance<pybind11::bool_>(obj))
        {
            value = xequation::EquationValue(rel::Value::Boolean(obj.cast<bool>()));
            return true;
        }
        if (pybind11::isinstance<pybind11::int_>(obj))
        {
            long long v = obj.cast<long long>();
            if (v < -2147483648LL || v > 2147483647LL)
            {
                throw pybind11::value_error("integer out of int32 range");
            }
            value = xequation::EquationValue(rel::Value::Integer(static_cast<int>(v)));
            return true;
        }
        if (pybind11::isinstance<pybind11::float_>(obj))
        {
            value = xequation::EquationValue(rel::Value::Real(obj.cast<double>()));
            return true;
        }
        if (pybind11::isinstance<pybind11::str>(obj))
        {
            value = xequation::EquationValue(rel::Value::String(obj.cast<std::string>()));
            return true;
        }

        // 已注册绑定的 rel.Value 实例（未来接入 REL Python 绑定时生效）
        if (pybind11::isinstance<rel::Value>(obj))
        {
            value = xequation::EquationValue(obj.cast<rel::Value>());
            return true;
        }

        // src 是借用引用，而 PyObjectRef 偷引用（不 incref）：
        // 必须先自增，否则源 py::object 析构后 PyObjectRef 悬垂
        // （use-after-free，Py_Finalize 时崩溃）。
        Py_XINCREF(obj.ptr());
        value = xequation::EquationValue(xequation::PyObjectRef(obj.ptr()));
        return true;
    }

    // EquationValue -> py::object
    static handle cast(const xequation::EquationValue &src, return_value_policy /*policy*/, handle /*parent*/)
    {
        if (src.IsNull())
        {
            return pybind11::none().release();
        }

        if (src.IsPyObject())
        {
            PyObject *p = src.AsPyObject().get();
            Py_XINCREF(p);
            return p;
        }

        // rel::Value：Measurement 标量 → 原生 Python 类型
        const rel::Value &rv = src.AsRel();
        if (rv.is_measurement() && rv.is_scalar())
        {
            const xdataset::Measurement &m = rv.as_measurement();
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return pybind11::cast(m.as_scalar<int>()).release();
                case xdataset::DataType::kReal:
                    return pybind11::cast(m.as_scalar<double>()).release();
                case xdataset::DataType::kBoolean:
                    return pybind11::cast(m.as_scalar<bool>()).release();
                case xdataset::DataType::kString:
                    return pybind11::cast(m.as_scalar<std::string>()).release();
                case xdataset::DataType::kComplex:
                    return pybind11::cast(m.as_scalar<std::complex<double>>()).release();
            }
        }

        // 非标量（vector/matrix/DataArray）：暂以 Format() 字符串表达
        return pybind11::cast(rv.Format()).release();
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE
