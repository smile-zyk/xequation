#pragma once

#include "function/function.h"  // FunctionParam / NativeFunction
#include "value.h"          // rel::Value

#include <sstream>
#include <string>
#include <vector>

namespace rel {

class Environment;

/// Parse and evaluate a single REL expression from a source string.
/// When `env` is nullptr (the default), a temporary Environment is used.
/// Otherwise the given Environment is used (with its variables, datasets,
/// and built-in constants).
/// Throws std::runtime_error on parse or evaluation failure.
Value Eval(const std::string& source, Environment* env = nullptr);

// =========================================================================
//  Value formatting helpers
// =========================================================================
//
//  Shared by REL's builtins (e.g. what(x)) and by hosts that want the same
//  text rendering of xdataset values.  All are inline so they are usable
//  from any translation unit without a rel_core link.

/// Render a MultiDimensionSpec as "[1, 2, [1, 2, 3]]" — regular dimensions
/// print their size, ragged dimensions nest their sizes.
inline std::string FormatDimensionSpec(const xdataset::MultiDimensionSpec& spec)
{
    std::ostringstream oss;
    oss << '[';
    const std::vector<xdataset::DimensionSpec>& dims = spec.dims();
    for (std::size_t i = 0; i < dims.size(); ++i)
    {
        if (i > 0) oss << ", ";
        const xdataset::DimensionSpec& d = dims[i];
        if (d.is_regular())
        {
            oss << d.regular_size();
        }
        else
        {
            oss << '[';
            const std::vector<std::size_t>& sizes = d.ragged_sizes();
            for (std::size_t j = 0; j < sizes.size(); ++j)
            {
                if (j > 0) oss << ", ";
                oss << sizes[j];
            }
            oss << ']';
        }
    }
    oss << ']';
    return oss.str();
}

/// Render a cell shape as "Scalar" / "Vector(w)" / "Matrix(r, c)".
inline std::string FormatDataShape(xdataset::DataKind kind,
                                   const xdataset::DataShape& shape)
{
    std::ostringstream oss;
    switch (kind)
    {
        case xdataset::DataKind::kScalar:
            oss << "Scalar";
            break;
        case xdataset::DataKind::kVector:
            oss << "Vector(" << shape[0] << ")";
            break;
        case xdataset::DataKind::kMatrix:
            oss << "Matrix(" << shape[0] << ", " << shape[1] << ")";
            break;
    }
    return oss.str();
}

/// Render a cell data type as Integer / Double / Complex / String / Boolean.
inline std::string FormatDataType(xdataset::DataType type)
{
    switch (type)
    {
        case xdataset::DataType::kInteger: return "Integer";
        case xdataset::DataType::kReal:    return "Double";
        case xdataset::DataType::kComplex: return "Complex";
        case xdataset::DataType::kString:  return "String";
        case xdataset::DataType::kBoolean: return "Boolean";
    }
    return "Unknown";
}

/// Render a unit to its human-readable string (e.g. "V", "GHz", "m/s").
/// Returns empty string when the unit has no physical dimension.
inline std::string FormatUnit(const xdataset::Unit& unit)
{
    return unit.to_string();
}

} // namespace rel
