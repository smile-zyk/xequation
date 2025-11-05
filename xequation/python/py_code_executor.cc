// Copyright 2024 Your Company. All rights reserved.

#include "py_code_executor.h"
#include "core/value.h"

namespace xequation
{
namespace python
{
const char kExecutorPythonCode[] = R"(
import builtins

class PyCodeExecutor:
    def exec(self, code_string, local_dict):
        # Execute code
        exec(code_string, local_dict)
    
    def eval(self, expression, local_dict):
        # Evaluate expression
        return eval(expression, local_dict)
)";

PyCodeExecutor::PyCodeExecutor()
{
    py::exec(kExecutorPythonCode);

    py::module main = py::module::import("__main__");
    py::object python_class = main.attr("PyCodeExecutor");
    executor_ = python_class();
}

PyCodeExecutor::~PyCodeExecutor()
{
    executor_.release();
}

Equation::Status PyCodeExecutor::MapPythonExceptionToStatus(const py::error_already_set &e)
{
    // Extract Python exception type
    py::object type = e.type();
    py::object type_name = type.attr("__name__");
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

ExecResult PyCodeExecutor::Exec(const std::string &code_string, const py::dict &local_dict)
{
    ExecResult res;
    try
    {
        executor_.attr("exec")(code_string, local_dict);
        res.status = Equation::Status::kSuccess;
        res.message = "";
    }
    catch (const py::error_already_set &e)
    {
        Equation::Status status = MapPythonExceptionToStatus(e);
        res.status = status;
        res.message = e.what();
    }
    return res;
}

EvalResult PyCodeExecutor::Eval(const std::string &expression, const py::dict &local_dict)
{
    EvalResult res;
    try
    {
        py::object result = executor_.attr("eval")(expression, local_dict);
        res.value = result;
        res.status = Equation::Status::kSuccess;
        res.message = "";
    }
    catch (const py::error_already_set &e)
    {
        Equation::Status status = MapPythonExceptionToStatus(e);
        res.value = Value::Null();
        res.status = status;
        res.message = e.what();
    }
    return res;
}
} // namespace python
} // namespace xequation