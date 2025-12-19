// Copyright 2024 Your Company. All rights reserved.

#include "python_executor.h"
#include "core/equation_common.h"
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

InterpretResult PythonExecutor::Exec(const std::string &code_string, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    InterpretResult res;
    res.mode = InterpretMode::kExec;
    try
    {
        executor_.attr("exec")(code_string, local_dict);
        res.status = ResultStatus::kSuccess;
    }
    catch (const pybind11::error_already_set &e)
    {
        ResultStatus status = MapPythonExceptionToStatus(e);
        res.status = status;
        pybind11::object pv = e.value();
        pybind11::object str_func = pybind11::module_::import("builtins").attr("str");
        std::string error_msg = str_func(pv).cast<std::string>();
        res.message = error_msg;
    }
    return res;
}

InterpretResult PythonExecutor::Eval(const std::string &expression, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    InterpretResult res;
    res.mode = InterpretMode::kEval;
    try
    {
        pybind11::object result = executor_.attr("eval")(expression, local_dict);
        res.value = result;
        res.status = ResultStatus::kSuccess;
    }
    catch (const pybind11::error_already_set &e)
    {
        ResultStatus status = MapPythonExceptionToStatus(e);
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