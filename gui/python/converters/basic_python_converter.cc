#include "basic_python_converter.h"

#include <QtVariantPropertyManager>

#include "variable_property_manager.h"

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

bool ListPropertyConverter::CanConvert(pybind11::object obj) const
{
    return pybind11::isinstance<pybind11::list>(obj);
}

VariableProperty *ListPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::object obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);

    VariableProperty *property = manager->addProperty(name);
    pybind11::list list_obj = obj.cast<pybind11::list>();
    pybind11::size_t length = list_obj.size();
    QString value_str = QString("{size = %1}").arg(length);

    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    for (pybind11::size_t i = 0; i < length; ++i) {
        pybind11::object item_obj = list_obj[i];
        
        QString item_name = QString("[%1]").arg(i);
        VariableProperty *item_property = CreatePythonProperty(manager, item_name, item_obj);
        property->addSubProperty(item_property);
    }

    return property;
}

} // namespace python
} // namespace gui
} // namespace xequation