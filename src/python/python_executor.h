#pragma once

#include <string>
#include <functional>

#include "python_common.h"
#include "core/equation_common.h"

namespace xequation
{
namespace python
{
// 简单的输出回调类型
using OutputHandler = std::function<void(const std::string&)>;

class PythonExecutor {
 public:
  PythonExecutor();
  ~PythonExecutor();
  
  // Disallow copy and assign.
  PythonExecutor(const PythonExecutor&) = delete;
  PythonExecutor& operator=(const PythonExecutor&) = delete;
  
  // 设置输出处理函数（stdout和stderr都会调用）
  void SetOutputHandler(OutputHandler handler);
  
  // 清除输出处理函数
  void ClearOutputHandler();
  
  // Executes Python code string in the given local dictionary.
  InterpretResult Exec(const std::string& code_string, const pybind11::dict& local_dict = pybind11::dict());
  
  // Evaluates Python expression in the given local dictionary.
  InterpretResult Eval(const std::string& expression, const pybind11::dict& local_dict = pybind11::dict());
  
 private:
  OutputHandler output_handler_;
  pybind11::object output_class_;  // 缓存 Python 输出类，避免重复定义
};
} // namespace python
} // namespace xequation