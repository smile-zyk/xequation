#pragma once
#include <string>

// CPython 对象前向声明：core 层只持有裸指针，不依赖 Python.h。
struct _object;
typedef _object PyObject;

namespace xequation
{

// 由 python 层注入的 PyObject 操作。core 不直接调用 CPython API。
//   incref    —— 引用自增（原子，无需 GIL）
//   decref    —— 引用释放（需要 GIL：持有则立即释放，否则入队延迟到 GIL 边界）
//   to_string —— 生成可读字符串（需要 GIL，如 repr/str）
//   type_name —— 获取对象类型名（需要 GIL，如 __class__.__name__）
struct PyObjectOps
{
    void (*incref)(PyObject *) = nullptr;
    void (*decref)(PyObject *) = nullptr;
    std::string (*to_string)(PyObject *) = nullptr;
    std::string (*type_name)(PyObject *) = nullptr;
};

// 全局设置/获取（python 层在解释器初始化后调用一次）
void SetPyObjectOps(const PyObjectOps &ops) noexcept;
const PyObjectOps &GetPyObjectOps() noexcept;

// 安全释放一个引用：委托给注入的 decref（持有 GIL 时立即释放，否则延迟）
void DeferPyDecRef(PyObject *p) noexcept;

// 不透明 Python 对象引用（reference-counted 封装）。
// 拷贝 = 原子 incref（无需 GIL）；析构 = 延迟 decref（入队，GIL 边界冲刷）。
class PyObjectRef
{
  public:
    PyObjectRef() noexcept = default;
    explicit PyObjectRef(PyObject *p) noexcept : ptr_(p) {}  // 偷引用（调用方持有）
    PyObjectRef(const PyObjectRef &o) noexcept;
    PyObjectRef(PyObjectRef &&o) noexcept;
    PyObjectRef &operator=(const PyObjectRef &o) noexcept;
    PyObjectRef &operator=(PyObjectRef &&o) noexcept;
    ~PyObjectRef() noexcept;

    PyObject *get() const noexcept
    {
        return ptr_;
    }

    // 委托给注入的 to_string；未注入时返回占位符
    std::string ToString() const;

    // 委托给注入的 type_name（如 __class__.__name__）；未注入时返回占位符
    std::string TypeName() const;

  private:
    PyObject *ptr_ = nullptr;
};

} // namespace xequation
