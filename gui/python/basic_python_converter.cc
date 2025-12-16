#include "basic_python_converter.h"

#include <pybind11/pybind11.h>
#include "variable_property_manager.h"

namespace xequation
{
namespace gui
{
namespace python
{

// Static helper implementations
QString PythonPropertyConverter::GetTypeName(pybind11::handle obj, bool qualified)
{
    pybind11::gil_scoped_acquire acquire;

    try
    {
        pybind11::handle type_obj = pybind11::type::of(obj);
        pybind11::handle type_name_attr = type_obj.attr("__name__");
        std::string type_name_str = type_name_attr.cast<std::string>();

        if (qualified && pybind11::hasattr(type_obj, "__module__"))
        {
            pybind11::handle module_attr = type_obj.attr("__module__");
            std::string module_str = module_attr.cast<std::string>();

            if (!module_str.empty() && module_str != "builtins" && module_str != "__main__")
            {
                return QString("%1.%2")
                    .arg(QString::fromStdString(module_str))
                    .arg(QString::fromStdString(type_name_str));
            }
        }

        return QString::fromStdString(type_name_str);
    }
    catch (...)
    {
        return QString("object");
    }
}

QString PythonPropertyConverter::GetObjectStr(pybind11::handle obj)
{
    pybind11::gil_scoped_acquire acquire;

    try
    {
        pybind11::handle str_obj = pybind11::str(obj);
        std::string str_result = str_obj.cast<std::string>();
        return QString::fromStdString(str_result);
    }
    catch (...)
    {
        return QString("<error>");
    }
}

// DefaultPropertyConverter implementation
bool DefaultPropertyConverter::CanConvert(pybind11::handle) const
{
    return true;
}

VariableProperty *DefaultPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);
    QString value_str = PythonPropertyConverter::GetObjectStr(obj);

    VariableProperty *property = manager->addProperty(name);
    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    return property;
}

// BasicPropertyConverter implementation
bool BasicPropertyConverter::CanConvert(pybind11::handle obj) const
{
    pybind11::handle complex_type = pybind11::module_::import("builtins").attr("complex");
    return pybind11::isinstance<pybind11::int_>(obj) || pybind11::isinstance<pybind11::float_>(obj) ||
           pybind11::isinstance<pybind11::str>(obj) || pybind11::isinstance<pybind11::bytes>(obj) ||
           pybind11::isinstance<pybind11::bytearray>(obj) || pybind11::isinstance<pybind11::memoryview>(obj) ||
           pybind11::isinstance(obj, complex_type) || pybind11::isinstance<pybind11::bool_>(obj) || obj.is_none();
}

VariableProperty *BasicPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);
    QString value_str = PythonPropertyConverter::GetObjectStr(obj);

    VariableProperty *property = manager->addProperty(name);
    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    return property;
}

// ListPropertyConverter implementation
bool ListPropertyConverter::CanConvert(pybind11::handle obj) const
{
    return pybind11::isinstance<pybind11::list>(obj);
}

VariableProperty *
ListPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);

    VariableProperty *property = manager->addProperty(name);
    pybind11::list list_obj = obj.cast<pybind11::list>();
    pybind11::size_t length = list_obj.size();
    QString value_str = QString("[%1 items]").arg(length);

    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    for (pybind11::size_t i = 0; i < length; ++i)
    {
        pybind11::handle item_obj = list_obj[i];

        QString item_name = QString("[%1]").arg(i);
        VariableProperty *item_property = CreatePythonProperty(manager, item_name, item_obj);
        property->addSubProperty(item_property);
    }

    return property;
}

// TuplePropertyConverter implementation
bool TuplePropertyConverter::CanConvert(pybind11::handle obj) const
{
    return pybind11::isinstance<pybind11::tuple>(obj);
}

VariableProperty *
TuplePropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);

    VariableProperty *property = manager->addProperty(name);
    pybind11::tuple tuple_obj = obj.cast<pybind11::tuple>();
    pybind11::size_t length = tuple_obj.size();
    QString value_str = QString("(%1 items)").arg(length);

    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    for (pybind11::size_t i = 0; i < length; ++i)
    {
        pybind11::handle item_obj = tuple_obj[i];

        QString item_name = QString("[%1]").arg(i);
        VariableProperty *item_property = CreatePythonProperty(manager, item_name, item_obj);
        property->addSubProperty(item_property);
    }

    return property;
}

// SetPropertyConverter implementation
bool SetPropertyConverter::CanConvert(pybind11::handle obj) const
{
    return pybind11::isinstance<pybind11::set>(obj);
}

VariableProperty *
SetPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);

    VariableProperty *property = manager->addProperty(name);
    pybind11::set set_obj = obj.cast<pybind11::set>();
    pybind11::size_t size = set_obj.size();
    QString value_str = QString("{%1 items}").arg(size);

    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    pybind11::size_t index = 0;
    for (auto item : set_obj)
    {
        pybind11::handle item_obj = item;

        QString item_name = QString("[%1]").arg(index++);
        VariableProperty *item_property = CreatePythonProperty(manager, item_name, item_obj);
        property->addSubProperty(item_property);
    }

    return property;
}

// DictPropertyConverter implementation
bool DictPropertyConverter::CanConvert(pybind11::handle obj) const
{
    return pybind11::isinstance<pybind11::dict>(obj);
}

VariableProperty *
DictPropertyConverter::CreateProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);

    VariableProperty *property = manager->addProperty(name);
    pybind11::dict dict_obj = obj.cast<pybind11::dict>();
    pybind11::size_t size = dict_obj.size();
    QString value_str = QString("{%1 items}").arg(size);

    manager->setValue(property, value_str);
    manager->setType(property, type_name);

    for (auto item : dict_obj)
    {
        pybind11::handle key_obj = item.first;
        pybind11::handle value_obj = item.second;

        QString key_str = PythonPropertyConverter::GetObjectStr(key_obj);
        VariableProperty *item_property = manager->addProperty(key_str);
        
        VariableProperty *key_property = CreatePythonProperty(manager, "key", key_obj);
        VariableProperty *value_property = CreatePythonProperty(manager, "value", value_obj);
        item_property->addSubProperty(key_property);
        item_property->addSubProperty(value_property);
        
        property->addSubProperty(item_property);
    }
    return property;
}

// Helper function to create Python properties
VariableProperty *CreatePythonProperty(VariablePropertyManager *manager, const QString &name, pybind11::handle obj)
{
    // Registry of converters in priority order
    static std::vector<std::unique_ptr<PythonPropertyConverter>> converters;
    if (converters.empty())
    {
        converters.push_back(std::make_unique<BasicPropertyConverter>());
        converters.push_back(std::make_unique<ListPropertyConverter>());
        converters.push_back(std::make_unique<TuplePropertyConverter>());
        converters.push_back(std::make_unique<SetPropertyConverter>());
        converters.push_back(std::make_unique<DictPropertyConverter>());
        converters.push_back(std::make_unique<DefaultPropertyConverter>());
    }

    for (const auto &converter : converters)
    {
        if (converter->CanConvert(obj))
        {
            return converter->CreateProperty(manager, name, obj);
        }
    }

    return nullptr;
}

} // namespace python
} // namespace gui
} // namespace xequation
