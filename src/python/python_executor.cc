#include "python_executor.h"

namespace xequation
{
namespace python
{

PythonExecutor::PythonExecutor()
{
    // 定义 Python 输出捕获类一次，缓存起来
    try
    {
        pybind11::gil_scoped_acquire acquire;
        pybind11::dict main_dict = pybind11::module_::import("__main__").attr("__dict__");
        
        pybind11::exec(R"(
import sys
class _Output:
    def __init__(self, func):
        self.func = func
    def write(self, msg):
        if msg and msg != '\n':
            self.func(msg)
        return len(msg) if msg else 0
    def flush(self):
        pass
)", main_dict);
        
        output_class_ = main_dict["_Output"];
    }
    catch (const pybind11::error_already_set& e)
    {
        // 定义失败，后续会再尝试
    }
}

PythonExecutor::~PythonExecutor()
{
}

void PythonExecutor::SetOutputHandler(OutputHandler handler)
{
    output_handler_ = handler;
}

void PythonExecutor::ClearOutputHandler()
{
    output_handler_ = nullptr;
}

InterpretResult PythonExecutor::Exec(const std::string &code_string, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    InterpretResult res;
    res.mode = InterpretMode::kExec;
    
    pybind11::object old_stdout;
    pybind11::object old_stderr;
    
    // 如果设置了输出处理器，重定向 stdout/stderr
    if (output_handler_ && output_class_)
    {
        try
        {
            pybind11::object sys_module = pybind11::module_::import("sys");
            old_stdout = sys_module.attr("stdout");
            old_stderr = sys_module.attr("stderr");
            
            pybind11::object handler_obj = pybind11::cast(output_handler_);
            pybind11::object output_obj = output_class_(handler_obj);
            
            sys_module.attr("stdout") = output_obj;
            sys_module.attr("stderr") = output_obj;
        }
        catch (const pybind11::error_already_set& e)
        {
            // 设置失败，继续执行
        }
    }
    
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
    
    // 恢复原始 stdout/stderr
    if (output_handler_ && output_class_)
    {
        try
        {
            pybind11::object sys_module = pybind11::module_::import("sys");
            if (old_stdout)
                sys_module.attr("stdout") = old_stdout;
            if (old_stderr)
                sys_module.attr("stderr") = old_stderr;
        }
        catch (const pybind11::error_already_set& e)
        {
        }
    }
    
    return res;
}

InterpretResult PythonExecutor::Eval(const std::string &expression, const pybind11::dict &local_dict)
{
    pybind11::gil_scoped_acquire acquire;

    InterpretResult res;
    res.mode = InterpretMode::kEval;
    
    pybind11::object old_stdout;
    pybind11::object old_stderr;
    
    // 如果设置了输出处理器，重定向 stdout/stderr
    if (output_handler_ && output_class_)
    {
        try
        {
            pybind11::object sys_module = pybind11::module_::import("sys");
            old_stdout = sys_module.attr("stdout");
            old_stderr = sys_module.attr("stderr");
            
            pybind11::object handler_obj = pybind11::cast(output_handler_);
            pybind11::object output_obj = output_class_(handler_obj);
            
            sys_module.attr("stdout") = output_obj;
            sys_module.attr("stderr") = output_obj;
        }
        catch (const pybind11::error_already_set& e)
        {
            // 设置失败，继续执行
        }
    }
    
    try
    {
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
    
    // 恢复原始 stdout/stderr
    if (output_handler_ && output_class_)
    {
        try
        {
            pybind11::object sys_module = pybind11::module_::import("sys");
            if (old_stdout)
                sys_module.attr("stdout") = old_stdout;
            if (old_stderr)
                sys_module.attr("stderr") = old_stderr;
        }
        catch (const pybind11::error_already_set& e)
        {
        }
    }
    
    return res;
}
} // namespace python
} // namespace xequation