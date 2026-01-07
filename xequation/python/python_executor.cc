#include "python_executor.h"
#include "core/equation_common.h"
#include "core/value.h"

namespace xequation
{
namespace python
{

PythonExecutor::PythonExecutor()
{
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
        pybind11::exec(code_string.c_str(), local_dict);
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
        // store python thread state

        pybind11::object result = pybind11::eval(expression.c_str(), local_dict);
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