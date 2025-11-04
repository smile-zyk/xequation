// Copyright 2024 Your Company. All rights reserved.

#include "py_code_executor.h"

namespace xequation
{
namespace python
{
const char kExecutorPythonCode[] = R"(
import builtins

class PyCodeExecutor:
    def __init__(self):
        self.global_dict = {'__builtins__': builtins}
    
    def exec(self, code_string, local_dict):
        # Execute code
        exec(code_string, self.global_dict, local_dict)
    
    def eval(self, expression, local_dict):
        # Evaluate expression
        return eval(expression, self.global_dict, local_dict)
    
    def get_available_builtins(self):
        return list(self.global_dict['__builtins__'].__dict__.keys())
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

ExecStatus PyCodeExecutor::MapPythonExceptionToStatus(const py::error_already_set &e)
{
    // Extract Python exception type
    py::object type = e.type();
    py::object type_name = type.attr("__name__");
    std::string type_name_str = type_name.cast<std::string>();

    if (type_name_str == "SyntaxError")
    {
        return ExecStatus::kSyntaxError;
    }
    else if (type_name_str == "NameError")
    {
        return ExecStatus::kNameError;
    }
    else if (type_name_str == "TypeError")
    {
        return ExecStatus::kTypeError;
    }
    else if (type_name_str == "ZeroDivisionError")
    {
        return ExecStatus::kZeroDivisionError;
    }
    else if (type_name_str == "ValueError")
    {
        return ExecStatus::kValueError;
    }
    else if (type_name_str == "MemoryError")
    {
        return ExecStatus::kMemoryError;
    }
    else if (type_name_str == "OverflowError")
    {
        return ExecStatus::kOverflowError;
    }
    else if (type_name_str == "RecursionError")
    {
        return ExecStatus::kRecursionError;
    }
    else if (type_name_str == "IndexError")
    {
        return ExecStatus::kIndexError;
    }
    else if (type_name_str == "KeyError")
    {
        return ExecStatus::kKeyError;
    }
    else if (type_name_str == "AttributeError")
    {
        return ExecStatus::kAttributeError;
    }

    // Default to ValueError for unknown exception types
    return ExecStatus::kValueError;
}

ExecResult PyCodeExecutor::Exec(const std::string &code_string, const py::dict &local_dict)
{
    ExecResult res;
    try
    {
        executor_.attr("exec")(code_string, local_dict);
        res.status = ExecStatus::kSuccess;
        res.message = "";
    }
    catch (const py::error_already_set &e)
    {
        ExecStatus status = MapPythonExceptionToStatus(e);
        res.status = status;
        res.message = e.what();
    }
    return res;
}

ExecResult PyCodeExecutor::Eval(const std::string &expression, const py::dict &local_dict)
{
    ExecResult res;
    try
    {
        py::object result = executor_.attr("eval")(expression, local_dict);
        res.status = ExecStatus::kSuccess;
        res.message = "";
    }
    catch (const py::error_already_set &e)
    {
        ExecStatus status = MapPythonExceptionToStatus(e);
        res.status = status;
        res.message = e.what();
    }
    return res;
}

std::vector<std::string> PyCodeExecutor::GetAvailableBuiltins()
{
    try
    {
        py::list result = executor_.attr("get_available_builtins")();
        return result.cast<std::vector<std::string>>();
    }
    catch (const py::error_already_set &e)
    {
        throw std::runtime_error(e.what());
    }
}
} // namespace python
} // namespace xequation