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
    if(value.Type() == typeid(py::object) || value.Type() == typeid(py::handle))
    {
        return true;
    }
    return false;
}

ValueItem::UniquePtr
PythonDefaultItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = py::cast(value);
    auto item = ValueItem::Create(name, value, parent);
    item->set_type(GetTypeName(obj));
    item->set_display_value(GetObjectRepr(obj));
    return item;
}

QString PythonDefaultItemBuilder::GetTypeName(py::handle obj, bool qualified)
{
    py::gil_scoped_acquire acquire;

    try
    {
        py::handle type_obj = py::type::of(obj);
        py::handle type_name_attr = type_obj.attr("__name__");
        std::string type_name_str = type_name_attr.cast<std::string>();

        if (qualified && py::hasattr(type_obj, "__module__"))
        {
            py::handle module_attr = type_obj.attr("__module__");
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

QString PythonDefaultItemBuilder::GetObjectRepr(py::handle obj)
{
    py::gil_scoped_acquire acquire;

    try
    {
        py::handle repr_obj = py::repr(obj);
        std::string repr_result = repr_obj.cast<std::string>();
        return QString::fromStdString(repr_result);
    }
    catch (const std::exception &e)
    {
        try
        {
            py::handle str_obj = py::str(obj);
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
    if(value.Type() == typeid(py::object) || value.Type() == typeid(py::handle))
    {
        auto obj = py::cast(value);
        return py::isinstance<py::list>(obj);
    }
    if(value.Type() == typeid(py::list))
    {
        return true;
    }
    return false;
}

ValueItem::UniquePtr PythonListItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = py::cast(value);
    auto list = py::cast<py::list>(obj);
    auto item = ValueItem::Create(name, value, parent);
    py::size_t length = list.size();
    QString value_str = QString("{size = %1}").arg(length);
    item->set_display_value(value_str);
    item->set_type(GetTypeName(obj));
    item->set_about_to_load_child_count(length);
    return item;
}

void PythonListItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = py::cast(item->value());
    auto list = py::cast<py::list>(obj);

    for (size_t i = 0; i < list.size(); ++i)
    {
        QString key = QString("[%1]").arg(i);
        Value child_value(py::cast<py::object>(list[i]));
        auto child_item = BuilderUtils::CreateValueItem(key, child_value, item);
        if (child_item)
        {
            item->AddChild(std::move(child_item));
        }
    }
}

// PythonTupleItemBuilder implementation
bool PythonTupleItemBuilder::CanBuild(const Value &value)
{
    if(value.Type() == typeid(py::object) || value.Type() == typeid(py::handle))
    {
        auto obj = py::cast(value);
        return py::isinstance<py::tuple>(obj);
    }
    if(value.Type() == typeid(py::tuple))
    {
        return true;
    }
    return false;
}

ValueItem::UniquePtr PythonTupleItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = py::cast(value);
    auto tuple = py::cast<py::tuple>(obj);
    auto item = ValueItem::Create(name, value, parent);
    py::size_t length = tuple.size();
    QString value_str = QString("{size = %1}").arg(length);
    item->set_display_value(value_str);
    item->set_type(GetTypeName(obj));
    item->set_about_to_load_child_count(length);
    return item;
}

void PythonTupleItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = py::cast(item->value());
    auto tuple = py::cast<py::tuple>(obj);
    for (size_t i = 0; i < tuple.size(); ++i)
    {
        QString key = QString("[%1]").arg(i);
        Value child_value(py::cast<py::object>(tuple[i]));
        auto child_item = BuilderUtils::CreateValueItem(key, child_value, item);
        if (child_item)
        {
            item->AddChild(std::move(child_item));
        }
    }
}

// PythonSetItemBuilder implementation
bool PythonSetItemBuilder::CanBuild(const Value &value)
{
    if(value.Type() == typeid(py::object) || value.Type() == typeid(py::handle))
    {
        auto obj = py::cast(value);
        return py::isinstance<py::set>(obj);
    }
    if(value.Type() == typeid(py::set))
    {
        return true;
    }
    return false;
}

ValueItem::UniquePtr PythonSetItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = py::cast(value);
    auto set = py::cast<py::set>(obj);
    auto item = ValueItem::Create(name, value, parent);
    py::size_t length = set.size();
    QString value_str = QString("{size = %1}").arg(length);
    item->set_display_value(value_str);
    item->set_type(GetTypeName(obj));
    item->set_about_to_load_child_count(length);
    return item;
}

void PythonSetItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = py::cast(item->value());
    auto set = py::cast<py::set>(obj);
    py::size_t index = 0;
    for (auto set_item : set)
    {
        QString item_name = QString("[%1]").arg(index++);
        ValueItem::UniquePtr child_item = BuilderUtils::CreateValueItem(item_name, set_item, item);
        item->AddChild(std::move(child_item));
    }
}

// PythonDictItemBuilder implementation
bool PythonDictItemBuilder::CanBuild(const Value &value)
{
    if(value.Type() == typeid(py::object) || value.Type() == typeid(py::handle))
    {
        auto obj = py::cast(value);
        return py::isinstance<py::dict>(obj);
    }
    if(value.Type() == typeid(py::dict))
    {
        return true;
    }
    return false;
}

ValueItem::UniquePtr PythonDictItemBuilder::CreateValueItem(const QString &name, const Value &value, ValueItem *parent)
{
    auto obj = py::cast(value);
    auto dict = py::cast<py::dict>(obj);
    auto item = ValueItem::Create(name, value, parent);
    py::size_t length = dict.size();
    QString value_str = QString("{size = %1}").arg(length);
    item->set_display_value(value_str);
    item->set_type(GetTypeName(obj));
    item->set_about_to_load_child_count(length);
    return item;
}

void PythonDictItemBuilder::LoadChildren(ValueItem *item)
{
    if (!item || item->is_loaded())
    {
        return;
    }

    auto obj = py::cast(item->value());
    auto dict = py::cast<py::dict>(obj);
    py::size_t index = 0;
    for (auto dict_item : dict)
    {
        py::handle key = dict_item.first;
        py::handle value = dict_item.second;

        QString key_str = PythonDefaultItemBuilder::GetObjectRepr(key);
        QString value_str = PythonDefaultItemBuilder::GetObjectRepr(value);
        ValueItem::UniquePtr child_item = BuilderUtils::CreateValueItem(key_str, value, item);
        item->AddChild(std::move(child_item));
    }
}

} // namespace gui
} // namespace xequation
