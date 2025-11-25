#include "basic_python_converter.h"

#include <QtVariantPropertyManager>

namespace xequation
{
namespace gui
{
namespace python
{
bool DefaultPythonPropertyConverter::CanConvert(pybind11::object) const
{
    return true;
}

bool BasicPythonPropertyConverter::CanConvert(pybind11::object obj) const
{
    pybind11::object complex_type = pybind11::module_::import("builtins").attr("complex");
    return pybind11::isinstance<pybind11::int_>(obj) || pybind11::isinstance<pybind11::float_>(obj) ||
           pybind11::isinstance<pybind11::str>(obj) || pybind11::isinstance<pybind11::bytes>(obj) ||
           pybind11::isinstance(obj, complex_type) || pybind11::isinstance<pybind11::bool_>(obj) || obj.is_none();
}

REGISTER_PYTHON_PROPERTY_CONVERTER(BasicPythonPropertyConverter, -100);
REGISTER_PYTHON_PROPERTY_CONVERTER(DefaultPythonPropertyConverter, 100);
} // namespace python
} // namespace gui
} // namespace xequation