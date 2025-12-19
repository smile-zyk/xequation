#pragma once

#include "core/equation_common.h"
#include "python_base.h"
#include "value_pybind_converter.h"

namespace xequation
{
namespace python
{
inline ResultStatus MapPythonExceptionToStatus(const pybind11::error_already_set &e)
{
    pybind11::gil_scoped_acquire acquire;

    // Extract Python exception type
    pybind11::object type = e.type();
    pybind11::object type_name = type.attr("__name__");
    std::string type_name_str = type_name.cast<std::string>();

    if (type_name_str == "SyntaxError")
    {
        return ResultStatus::kSyntaxError;
    }
    else if (type_name_str == "NameError")
    {
        return ResultStatus::kNameError;
    }
    else if (type_name_str == "TypeError")
    {
        return ResultStatus::kTypeError;
    }
    else if (type_name_str == "ZeroDivisionError")
    {
        return ResultStatus::kZeroDivisionError;
    }
    else if (type_name_str == "ValueError")
    {
        return ResultStatus::kValueError;
    }
    else if (type_name_str == "MemoryError")
    {
        return ResultStatus::kMemoryError;
    }
    else if (type_name_str == "OverflowError")
    {
        return ResultStatus::kOverflowError;
    }
    else if (type_name_str == "RecursionError")
    {
        return ResultStatus::kRecursionError;
    }
    else if (type_name_str == "IndexError")
    {
        return ResultStatus::kIndexError;
    }
    else if (type_name_str == "KeyError")
    {
        return ResultStatus::kKeyError;
    }
    else if (type_name_str == "AttributeError")
    {
        return ResultStatus::kAttributeError;
    }

    return ResultStatus::kUnknownError;
}
} // namespace python
} // namespace xequation