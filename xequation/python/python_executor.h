#pragma once

#include <string>

#include "python_common.h"
#include "core/equation_common.h"
#include "core/equation.h"

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
  ExecResult Exec(const std::string& code_string, const py::dict& local_dict = py::dict());
  
  // Evaluates Python expression in the given local dictionary.
  EvalResult Eval(const std::string& expression, const py::dict& local_dict = py::dict());
 private:
  py::object executor_;
  
  // Maps Python exception types to ExecStatus.
  Equation::Status MapPythonExceptionToStatus(const py::error_already_set& e);
};
} // namespace python
} // namespace xequation