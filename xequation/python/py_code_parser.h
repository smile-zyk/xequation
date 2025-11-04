#pragma once

#include <string>

#include "core/expr_common.h"
#include "py_common.h"


namespace xequation
{
namespace python
{

class PyCodeParser
{
  public:
    PyCodeParser();
    ~PyCodeParser();

    PyCodeParser(const PyCodeParser &) = delete;
    PyCodeParser &operator=(const PyCodeParser &) = delete;

    ParseResult Parse(const std::string &code);

  private:
    py::object parser_;
};
} // namespace python
} // namespace xequation