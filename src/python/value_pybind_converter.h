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
    // 所有 Python 对象原样保留为不透明 PyObjectRef（零归一化）：
    //   - 往返保真：同一 Python 对象进 C++ 再出去，是同一个引用；
    //   - load/cast 对称：Python 侧的值永不在此解包/归一化；
    //   - GUI builder 全覆盖（所有值都是 IsPyObject()）。
    // C++ 侧构造的 rel::Value 载荷仍可正常承载（见 cast 方向）。
    bool load(handle src, bool)
    {
        if (!src)
        {
            return false;
        }
        // src 是借用引用，而 PyObjectRef 偷引用（不 incref）：
        // 必须先自增，否则源 py::object 析构后 PyObjectRef 悬垂
        // （use-after-free，Py_Finalize 时崩溃）。
        Py_XINCREF(src.ptr());
        value = xequation::EquationValue(xequation::PyObjectRef(src.ptr()));
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

        // rel::Value：交给 REL 自己的 Python 绑定（rel.Value 对象，带完整
        // 运算符 / __str__ / buffer 协议），不在此解包成原生标量。
        // embedded 的 rel 模块首次 import 时才执行 register_rel_bindings，
        // 因此先确保它已导入（幂等，解释器缓存模块对象）。
        pybind11::module_::import("rel");
        return pybind11::cast(src.AsRel()).release();
    }
};

} // namespace detail
} // namespace PYBIND11_NAMESPACE
