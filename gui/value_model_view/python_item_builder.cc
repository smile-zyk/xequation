#include "python_item_builder.h"
#include "value_item.h"

namespace py = pybind11;

namespace xequation
{
namespace gui
{

// PythonDefaultItemBuilder implementation
PythonDefaultItemBuilder::PythonDefaultItemBuilder() = default;
PythonDefaultItemBuilder::~PythonDefaultItemBuilder() = default;

bool PythonDefaultItemBuilder::CanBuild(const Value &value)
{

    if (value.Type() != typeid(py::object))
    {
        return false;
    }
    return true;
}

ValueItem::UniquePtr
PythonDefaultItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    try
    {
        auto obj = value.Cast<py::object>();
        auto item = ValueItemBuilder::CreateValueItem(name, value, parent);
        SetDisplayValueString(item.get(), GetObjectStr(obj));
        SetTypeString(item.get(), GetTypeName(obj));
        return item;
    }
    catch (...)
    {
        return nullptr;
    }
}

QString PythonDefaultItemBuilder::GetTypeName(pybind11::handle obj, bool qualified)
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

QString PythonDefaultItemBuilder::GetObjectStr(pybind11::handle obj)
{
    pybind11::gil_scoped_acquire acquire;

    try
    {
        pybind11::handle repr_obj = pybind11::repr(obj);
        std::string repr_result = repr_obj.cast<std::string>();
        return QString::fromStdString(repr_result);
    }
    catch (const std::exception &e)
    {
        try
        {
            pybind11::handle str_obj = pybind11::str(obj);
            std::string str_result = str_obj.cast<std::string>();
            return QString::fromStdString(str_result);
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

// PythonListItemBuilder implementation
bool PythonListItemBuilder::CanBuild(const Value &value)
{
    if (PythonDefaultItemBuilder::CanBuild(value) == false)
    {
        return false;
    }
    auto obj = value.Cast<py::object>();
    return py::isinstance<py::list>(obj);
}

ValueItem::UniquePtr PythonListItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = value.Cast<py::object>();
    auto list = py::cast<py::list>(obj);
    auto item = ValueItemBuilder::CreateValueItem(name, value, parent);
    pybind11::size_t length = list.size();
    QString value_str = QString("{size = %1}").arg(length);
    SetDisplayValueString(item.get(), value_str);
    SetTypeString(item.get(), GetTypeName(obj));
    SetHasChildren(item.get(), length > 0);
    return item;
}

void PythonListItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = item->value().Cast<py::object>();
    auto list = py::cast<py::list>(obj);

    for (size_t i = 0; i < list.size(); ++i)
    {
        QString key = QString("[%1]").arg(i);
        Value child_value(py::cast<py::object>(list[i]));
        auto child_item = CreateValueItemByBuilder(key, child_value, item);
        if (child_item)
        {
            item->AddChild(std::move(child_item));
        }
    }
    SetIsLoaded(item, true);
}

// PythonTupleItemBuilder implementation
bool PythonTupleItemBuilder::CanBuild(const Value &value)
{
    if (PythonDefaultItemBuilder::CanBuild(value) == false)
    {
        return false;
    }
    auto obj = value.Cast<py::object>();
    return py::isinstance<py::tuple>(obj);
}

ValueItem::UniquePtr PythonTupleItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = value.Cast<py::object>();
    auto tuple = py::cast<py::tuple>(obj);
    auto item = ValueItemBuilder::CreateValueItem(name, value, parent);
    pybind11::size_t length = tuple.size();
    QString value_str = QString("{size = %1}").arg(length);
    SetDisplayValueString(item.get(), value_str);
    SetTypeString(item.get(), GetTypeName(obj));
    SetHasChildren(item.get(), length > 0);
    return item;
}

void PythonTupleItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = item->value().Cast<py::object>();
    auto tuple = py::cast<py::tuple>(obj);
    for (size_t i = 0; i < tuple.size(); ++i)
    {
        QString key = QString("[%1]").arg(i);
        Value child_value(py::cast<py::object>(tuple[i]));
        auto child_item = CreateValueItemByBuilder(key, child_value, item);
        if (child_item)
        {
            item->AddChild(std::move(child_item));
        }
    }
    SetIsLoaded(item, true);
}

// PythonSetItemBuilder implementation
bool PythonSetItemBuilder::CanBuild(const Value &value)
{
    if (PythonDefaultItemBuilder::CanBuild(value) == false)
    {
        return false;
    }
    auto obj = value.Cast<py::object>();
    return py::isinstance<py::set>(obj);
}

ValueItem::UniquePtr PythonSetItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = value.Cast<py::object>();
    auto set = py::cast<py::set>(obj);
    auto item = ValueItemBuilder::CreateValueItem(name, value, parent);
    pybind11::size_t length = set.size();
    QString value_str = QString("{size = %1}").arg(length);
    SetDisplayValueString(item.get(), value_str);
    SetTypeString(item.get(), GetTypeName(obj));
    SetHasChildren(item.get(), length > 0);
    return item;
}

void PythonSetItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = item->value().Cast<py::object>();
    auto set = py::cast<py::set>(obj);
    pybind11::size_t index = 0;
    for (auto set_item : set)
    {
        QString item_name = QString("[%1]").arg(index++);
        ValueItem::UniquePtr child_item = CreateValueItemByBuilder(item_name, set_item, item);
        item->AddChild(std::move(child_item));
    }
    SetIsLoaded(item, true);
}

// PythonDictItemBuilder implementation
bool PythonDictItemBuilder::CanBuild(const Value &value)
{
    if (PythonDefaultItemBuilder::CanBuild(value) == false)
    {
        return false;
    }
    auto obj = value.Cast<py::object>();
    return py::isinstance<py::dict>(obj);
}

ValueItem::UniquePtr PythonDictItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = value.Cast<py::object>();
    auto dict = py::cast<py::dict>(obj);
    auto item = ValueItemBuilder::CreateValueItem(name, value, parent);
    pybind11::size_t length = dict.size();
    QString value_str = QString("{size = %1}").arg(length);
    SetDisplayValueString(item.get(), value_str);
    SetTypeString(item.get(), GetTypeName(obj));
    SetHasChildren(item.get(), length > 0);
    return item;
}

void PythonDictItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = item->value().Cast<py::object>();
    auto dict = py::cast<py::dict>(obj);
    pybind11::size_t index = 0;
    for (auto dict_item : dict)
    {
        py::handle key = dict_item.first;
        py::handle value = dict_item.second;

        QString key_str = PythonDefaultItemBuilder::GetObjectStr(key);
        QString value_str = PythonDefaultItemBuilder::GetObjectStr(value);

        ValueItem::UniquePtr child_item = ValueItemBuilder::CreateValueItem(key_str, const Value &value))

        QString item_name = QString("[%1]").arg(index++);
        ValueItem::UniquePtr child_item = CreateValueItemByBuilder(item_name, set_item, item);
        item->AddChild(std::move(child_item));
    }
    SetIsLoaded(item, true);
}

} // namespace gui
} // namespace xequation
