#pragma once

#include <string>
#include <vector>

#include "py_common.h"
#include "core/expr_common.h"

namespace xequation
{
namespace python
{
class PyCodeExecutor {
 public:
  PyCodeExecutor();
  ~PyCodeExecutor();
  
  // Disallow copy and assign.
  PyCodeExecutor(const PyCodeExecutor&) = delete;
  PyCodeExecutor& operator=(const PyCodeExecutor&) = delete;
  
  // Executes Python code string in the given local dictionary.
  ExecResult Exec(const std::string& code_string, const py::dict& local_dict = py::dict());
  
  // Evaluates Python expression in the given local dictionary.
  ExecResult Eval(const std::string& expression, const py::dict& local_dict = py::dict());
  
  // Returns list of available builtin function names.
  std::vector<std::string> GetAvailableBuiltins();

 private:
  py::object executor_;
  
  // Maps Python exception types to ExecStatus.
  ExecStatus MapPythonExceptionToStatus(const py::error_already_set& e);
};
} // namespace python
} // namespace xequation