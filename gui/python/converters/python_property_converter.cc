#include "python_property_converter.h"

#include <algorithm>
#include <QtVariantPropertyManager>

namespace xequation
{
namespace gui
{
namespace python
{

QtVariantProperty *
PythonPropertyConverter::CreateProperty(QtVariantPropertyManager *manager, const QString &name, pybind11::object obj)
{
    QString type_name = PythonPropertyConverter::GetTypeName(obj);
    QString object_str = PythonPropertyConverter::GetObjectStr(obj);

    QtVariantProperty *property = manager->addProperty(QVariant::String, name);
    property->setValue(type_name + ": " + object_str);
    return property;
}

QString PythonPropertyConverter::GetTypeName(pybind11::object obj, bool qualified)
{
    pybind11::gil_scoped_acquire acquire;

    try
    {
        pybind11::handle type_obj = obj.get_type();
        pybind11::object type_name_attr = type_obj.attr("__name__");
        std::string type_name_str = type_name_attr.cast<std::string>();

        if (qualified && pybind11::hasattr(type_obj, "__module__"))
        {
            pybind11::object module_attr = type_obj.attr("__module__");
            std::string module_str = module_attr.cast<std::string>();

            if (!module_str.empty() && module_str != "builtins" && module_str != "__main__" && module_str != "builtin")
            {
                return QString("%1.%2")
                    .arg(QString::fromStdString(module_str))
                    .arg(QString::fromStdString(type_name_str));
            }
        }

        return QString::fromStdString(type_name_str);
    }
    catch (const std::exception &e)
    {
        return QString("UnknownType[Error: %1]").arg(e.what());
    }
    catch (...)
    {
        return QString("UnknownType[Unknown error]");
    }
}

QString PythonPropertyConverter::GetObjectStr(pybind11::object obj)
{
    pybind11::gil_scoped_acquire acquire;

    try
    {
        pybind11::object str_obj = pybind11::str(obj);
        std::string str_result = str_obj.cast<std::string>();

        return QString::fromStdString(str_result);
    }
    catch (const std::exception &e)
    {
        try
        {
            pybind11::object repr_obj = pybind11::repr(obj);
            std::string repr_result = repr_obj.cast<std::string>();
            return QString::fromStdString(repr_result);
        }
        catch (const std::exception &e2)
        {
            return QString("[Error getting string representation: %1]").arg(e.what());
        }
    }
    catch (...)
    {
        return QString("[Unknown error getting string representation]");
    }
}

PythonPropertyConverterRegistry &PythonPropertyConverterRegistry::GetInstance()
{
    static PythonPropertyConverterRegistry instance;
    return instance;
}

void PythonPropertyConverterRegistry::RegisterConverter(
    std::unique_ptr<PythonPropertyConverter> converter, int priority
)
{
    ConverterEntry entry{std::move(converter), priority};

    auto it = std::lower_bound(converters_.begin(), converters_.end(), entry);
    converters_.insert(it, std::move(entry));
}

void PythonPropertyConverterRegistry::UnRegisterConverter(PythonPropertyConverter *converter)
{
    auto it = std::find_if(converters_.begin(), converters_.end(), [converter](const ConverterEntry &entry) {
        return entry.converter.get() == converter;
    });

    if (it != converters_.end())
    {
        converters_.erase(it);
    }
}

PythonPropertyConverter *PythonPropertyConverterRegistry::FindConverter(pybind11::object obj)
{
    for (const auto &entry : converters_)
    {
        if (entry.converter->CanConvert(obj))
        {
            return entry.converter.get();
        }
    }
    return nullptr;
}

QtVariantProperty *PythonPropertyConverterRegistry::CreateProperty(
    QtVariantPropertyManager *manager, const QString &name, pybind11::object obj
)
{
    for (const auto &entry : converters_)
    {
        if (entry.converter->CanConvert(obj))
        {
            return entry.converter->CreateProperty(manager, name, obj);
        }
    }
    return nullptr;
}

void PythonPropertyConverterRegistry::Clear()
{
    converters_.clear();
}

} // namespace python
} // namespace gui
} // namespace xequation