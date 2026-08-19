#include "equation_value.h"

namespace xequation
{

namespace
{
PyObjectOps g_py_ops;
}

void SetPyObjectOps(const PyObjectOps &ops) noexcept
{
    g_py_ops = ops;
}

const PyObjectOps &GetPyObjectOps() noexcept
{
    return g_py_ops;
}

void DeferPyDecRef(PyObject *p) noexcept
{
    if (!p)
        return;
    const PyObjectOps &ops = g_py_ops;
    if (ops.decref)
    {
        ops.decref(p);
    }
    // 未注入（python 层未初始化）：无法安全释放，放弃引用计数管理，
    // 由进程退出时 Python 解释器的回收兜底。
}

PyObjectRef::PyObjectRef(const PyObjectRef &o) noexcept : ptr_(o.ptr_)
{
    if (ptr_ && g_py_ops.incref)
    {
        g_py_ops.incref(ptr_);
    }
}

PyObjectRef::PyObjectRef(PyObjectRef &&o) noexcept : ptr_(o.ptr_)
{
    o.ptr_ = nullptr;
}

PyObjectRef &PyObjectRef::operator=(const PyObjectRef &o) noexcept
{
    if (this != &o)
    {
        PyObject *old = ptr_;
        ptr_ = o.ptr_;
        if (ptr_ && g_py_ops.incref)
        {
            g_py_ops.incref(ptr_);
        }
        DeferPyDecRef(old);
    }
    return *this;
}

PyObjectRef &PyObjectRef::operator=(PyObjectRef &&o) noexcept
{
    if (this != &o)
    {
        DeferPyDecRef(ptr_);
        ptr_ = o.ptr_;
        o.ptr_ = nullptr;
    }
    return *this;
}

PyObjectRef::~PyObjectRef() noexcept
{
    DeferPyDecRef(ptr_);
}

std::string PyObjectRef::ToString() const
{
    if (ptr_ && g_py_ops.to_string)
    {
        return g_py_ops.to_string(ptr_);
    }
    return "<python object>";
}

EquationValue::Kind EquationValue::kind() const noexcept
{
    return static_cast<Kind>(storage_.which());
}

bool EquationValue::IsInteger() const
{
    return IsRelValue() && AsRel().is_measurement() &&
           AsRel().data_type() == xdataset::DataType::kInteger;
}

bool EquationValue::IsReal() const
{
    return IsRelValue() && AsRel().is_measurement() &&
           AsRel().data_type() == xdataset::DataType::kReal;
}

bool EquationValue::IsBoolean() const
{
    return IsRelValue() && AsRel().is_measurement() &&
           AsRel().data_type() == xdataset::DataType::kBoolean;
}

bool EquationValue::IsString() const
{
    return IsRelValue() && AsRel().is_measurement() &&
           AsRel().data_type() == xdataset::DataType::kString;
}

std::string EquationValue::ToString() const
{
    if (IsNull())
    {
        return "null";
    }
    if (IsRelValue())
    {
        return AsRel().Format();
    }
    return AsPyObject().ToString();
}

} // namespace xequation
