#pragma once

#include <string>

#include "python_common.h"
#include "core/equation_common.h"

namespace xequation
{
namespace python
{
class PythonExecutor {
 public:
  PythonExecutor();
  ~PythonExecutor();
  
  // Disallow copy and assign.
  PythonExecutor(const PythonExecutor&) = delete;
  PythonExecutor& operator=(const PythonExecutor&) = delete;
  
  // Executes Python code string in the given local dictionary.
  InterpretResult Exec(const std::string& code_string, const pybind11::dict& local_dict = pybind11::dict());
  
  // Evaluates Python expression in the given local dictionary.
  InterpretResult Eval(const std::string& expression, const pybind11::dict& local_dict = pybind11::dict());
 private:
  pybind11::object executor_;
};
} // namespace python
} // namespace xequation