// Copyright 2024 Your Company. All rights reserved.

#include "python_executor.h"
#include "core/value.h"

namespace xequation
{
namespace python
{
const char kExecutorPythonCode[] = R"(
import builtins

class PythonExecutor:
    def exec(self, code_string, local_dict):
        # Execute code
        exec(code_string, local_dict)
    
    def eval(self, expression, local_dict):
        # Evaluate expression
        return eval(expression, local_dict)
)";

PythonExecutor::PythonExecutor()
{
    pybind11::gil_scoped_acquire acquire;

    pybind11::exec(kExecutorPythonCode);

    pybind11::module main = pybind11::module::import("__main__");
    pybind11::object python_class = main.attr("PythonExecutor");
    executor_ = python_class();
}

PythonExecutor::~PythonExecutor()
{
}

Equation::Status PythonExecutor::MapPythonExceptionToStatus(const pybind11::error_already_set &e)
{
    pybind11::gil_scoped_acquire acquire;

    // Extract Python exception type
    pybind11::object type = e.type();
    pybind11::object type_name = type.attr("__name__");
    std::string type_name_str = type_name.cast<std::string>();

    if (type_name_str == "SyntaxError")
    {
        return Equation::Status::kSyntaxError;
    }
    else if (type_name_str == "NameError")
    {
        return Equation::Status::kNameError;
    }
    else if (type_name_str == "TypeError")
    {
        return Equation::Status::kTypeError;
    }
    else if (type_name_str == "ZeroDivisionError")
    {
        return Equation::Status::kZeroDivisionError;
    }
    else if (type_name_str == "ValueError")
    {
        return Equation::Status::kValueError;
    }
    else if (type_name_str == "MemoryError")
    {
        return Equation::Status::kMemoryError;
    }
    else if (type_name_str == "OverflowError")
    {
        return Equation::Status::kOverflowError;
    }
    else if (type_name_str == "RecursionError")
    {
        return Equation::Status::kRecursionError;
    }
    else if (type_name_str == "IndexError")
    {
        return Equation::Status::kIndexError;
    }
    else if (type_name_str == "KeyError")
    {
        return Equation::Status::kKeyError;
    }
    else if (type_name_str == "AttributeError")
    {
        return Equation::Status::kAttributeError;
    }

    // Default to ValueError for unknown exception types
    return Equation::Status::kValueError;
}

ExecResult PythonExecutor::Exec(const std::string &code_string, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    ExecResult res;
    try
    {
        executor_.attr("exec")(code_string, local_dict);
        res.status = Equation::Status::kSuccess;
        res.message = "";
    }
    catch (const pybind11::error_already_set &e)
    {
        Equation::Status status = MapPythonExceptionToStatus(e);
        res.status = status;
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        res.message = error_msg;
    }
    return res;
}

EvalResult PythonExecutor::Eval(const std::string &expression, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    EvalResult res;
    try
    {
        pybind11::object result = executor_.attr("eval")(expression, local_dict);
        res.value = result;
        res.status = Equation::Status::kSuccess;
        res.message = "";
    }
    catch (const pybind11::error_already_set &e)
    {
        Equation::Status status = MapPythonExceptionToStatus(e);
        res.value = Value::Null();
        res.status = status;
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        res.message = error_msg;
    }
    return res;
}
} // namespace python
} // namespace xequation