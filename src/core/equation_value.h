#pragma once

#include <boost/optional.hpp>
#include <string>
#include <utility>

#include "value.h"  // rel::Value（来自 REL rel_runtime）

namespace xequation
{

// =========================================================================
//  EquationValue —— 名字绑定/求值结果的值类型（C++11 / Boost）
//
//  内部是一个 boost::optional<rel::Value>：
//    null  —— 无值（依赖缺失 / 尚未计算 / 求值失败）
//    值    —— rel::Value（标量/向量/矩阵/DataArray，含 unit）
//
//  rel::Value 本身没有 null 概念（默认是 Measurement Integer 0），而依赖
//  管理器需要表达"名字未绑定"，因此用 optional 承载空态。
//
//  包装只提供状态查询与底层访问：标量提取、类型检查等宿主直接用
//  Value() 拿到的 rel::Value 完成（as_measurement / data_type ...），
//  这里不再重复提供便捷方法。
// =========================================================================
class EquationValue
{
  public:
    // ---- 构造 ---------------------------------------------------------

    /// null（无值）。
    EquationValue() noexcept : storage_(boost::none) {}
    /// null（无值），显式 none_t 构造。
    EquationValue(boost::none_t) noexcept : storage_(boost::none) {}

    /// 从 rel::Value 构造。
    EquationValue(const rel::Value &v) : storage_(v) {}
    EquationValue(rel::Value &&v) : storage_(std::move(v)) {}

    // 便捷标量构造（映射到 rel::Value）。
    EquationValue(int v) : storage_(rel::Value::Integer(v)) {}
    EquationValue(double v) : storage_(rel::Value::Real(v)) {}
    EquationValue(bool v) : storage_(rel::Value::Boolean(v)) {}
    EquationValue(const char *v) : storage_(rel::Value::String(std::string(v))) {}
    EquationValue(const std::string &v) : storage_(rel::Value::String(v)) {}

    // ---- 状态查询 -----------------------------------------------------

    /// 是否无值。
    bool IsNull() const noexcept
    {
        return !storage_;
    }
    /// 是否持有值（rel::Value）。
    bool HasValue() const noexcept
    {
        return storage_.is_initialized();
    }

    // ---- 访问 ---------------------------------------------------------

    /// 取底层 rel::Value；仅当 HasValue() 时调用。
    const rel::Value &Value() const
    {
        return *storage_;
    }
    rel::Value &Value()
    {
        return *storage_;
    }

  private:
    boost::optional<rel::Value> storage_;
};

} // namespace xequation
