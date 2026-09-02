#pragma once

// 测试用 EquationValue 便捷工具（不进入生产代码）。
// EquationValue 只暴露 IsNull/HasValue/Value()；标量提取与类型检查
// 在这里集中，测试断言保持可读。

#include "core/equation_value.h"

namespace xequation
{

/// 提取 Measurement 标量。空值 / 非 Measurement 抛 std::runtime_error。
template <typename T>
T AsScalar(const EquationValue &v)
{
    return v.Value().as_measurement().as_scalar<T>();
}

inline bool IsIntegerValue(const EquationValue &v)
{
    return v.HasValue() && v.Value().is_measurement() &&
           v.Value().data_type() == xdataset::DataType::kInteger;
}

inline bool IsRealValue(const EquationValue &v)
{
    return v.HasValue() && v.Value().is_measurement() &&
           v.Value().data_type() == xdataset::DataType::kReal;
}

inline bool IsBooleanValue(const EquationValue &v)
{
    return v.HasValue() && v.Value().is_measurement() &&
           v.Value().data_type() == xdataset::DataType::kBoolean;
}

inline bool IsStringValue(const EquationValue &v)
{
    return v.HasValue() && v.Value().is_measurement() &&
           v.Value().data_type() == xdataset::DataType::kString;
}

/// 紧凑文本：null（无值）或底层 rel::Value 的 Format()。
inline std::string ValueToString(const EquationValue &v)
{
    return v.HasValue() ? v.Value().Format() : std::string("null");
}

} // namespace xequation
