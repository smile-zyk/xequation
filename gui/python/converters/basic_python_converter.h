#pragma once
#include "python_property_converter.h"

namespace xequation
{
namespace gui
{
namespace python
{
class DefaultPythonPropertyConverter : public PythonPropertyConverter
{
  public:
    bool CanConvert(pybind11::object obj) const override;
};

class BasicPythonPropertyConverter : public PythonPropertyConverter
{
  public:
    bool CanConvert(pybind11::object obj) const override;
};

class ListPropertyConverter : public PythonPropertyConverter
{
  public:
    bool CanConvert(pybind11::object obj) const override;
    VariableProperty *CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::object obj) override;
};
} // namespace python
} // namespace gui
} // namespace xequation