#include "python_equation_context.h"
#include "core/value.h"

using namespace xequation;
using namespace xequation::python;

PythonEquationContext::PythonEquationContext(const EquationEngineInfo &engine_info)
{
    engine_info_ = engine_info;
    pybind11::gil_scoped_acquire acquire;
    dict_.reset(new pybind11::dict());
    (*dict_)["__builtins__"] = pybind11::module_::import("builtins");
}

Value PythonEquationContext::Get(const std::string &var_name) const
{
    pybind11::gil_scoped_acquire acquire;
    if (dict_->contains(var_name))
    {
        Value value = pybind11::cast<Value>((*dict_)[var_name.c_str()]);
        return value;
    }
    return Value::Null();
}

bool PythonEquationContext::Contains(const std::string &var_name) const
{
    pybind11::gil_scoped_acquire acquire;
    return dict_->contains(var_name);
}

std::unordered_set<std::string> PythonEquationContext::keys() const
{
    pybind11::gil_scoped_acquire acquire;

    pybind11::object keys_obj = dict_->attr("keys")();
    std::unordered_set<std::string> keys;

    for (auto key : keys_obj)
    {
        keys.insert(key.cast<std::string>());
    }
    return keys;
}

void PythonEquationContext::Set(const std::string &var_name, const Value &value)
{
    pybind11::gil_scoped_acquire acquire;

    (*dict_)[var_name.c_str()] = value;
}

bool PythonEquationContext::Remove(const std::string &var_name)
{
    pybind11::gil_scoped_acquire acquire;

    if (dict_->contains(var_name))
    {
        dict_->attr("__delitem__")(var_name);
        return true;
    }
    return false;
}

void PythonEquationContext::Clear()
{
    pybind11::gil_scoped_acquire acquire;

    dict_.reset(new pybind11::dict());
    (*dict_)["__builtins__"] = pybind11::module_::import("builtins");
}

size_t PythonEquationContext::size() const
{
    pybind11::gil_scoped_acquire acquire;

    return dict_->size();
}

bool PythonEquationContext::empty() const
{
    pybind11::gil_scoped_acquire acquire;

    return dict_->empty();
}

std::vector<std::string> PythonEquationContext::GetBuiltinNames() const
{
    pybind11::gil_scoped_acquire acquire;

    std::vector<std::string> names;

    if (dict_->contains("__builtins__"))
    {
        pybind11::object builtins_module = (*dict_)["__builtins__"];
        pybind11::dict builtins_dict = builtins_module.attr("__dict__");

        for (auto item : builtins_dict)
        {
            names.push_back(item.first.cast<std::string>());
        }
    }

    return names;
}

std::vector<std::string> PythonEquationContext::GetSymbolNames() const
{
    pybind11::gil_scoped_acquire acquire;

    std::vector<std::string> names;

    for (auto item : *dict_)
    {
        names.push_back(item.first.cast<std::string>());
    }

    return names;
}

std::string PythonEquationContext::GetSymbolType(const std::string &symbol_name) const
{
    pybind11::gil_scoped_acquire acquire;

    std::string type;

    // First check in the main dict
    if (dict_->contains(symbol_name))
    {
        pybind11::object obj = (*dict_)[symbol_name.c_str()];
        pybind11::object type_obj = obj.attr("__class__");
        pybind11::object type_name = type_obj.attr("__name__");
        type = type_name.cast<std::string>();
    }
    else if (dict_->contains("__builtins__"))
    {
        pybind11::object builtins_module = (*dict_)["__builtins__"];
        pybind11::dict builtins_dict = builtins_module.attr("__dict__");

        if (builtins_dict.contains(symbol_name))
        {
            pybind11::object obj = builtins_dict[symbol_name.c_str()];
            pybind11::object type_obj = obj.attr("__class__");
            pybind11::object type_name = type_obj.attr("__name__");
            type = type_name.cast<std::string>();
        }
    }

    if (type == "builtin_function_or_method" || type == "builtin_method")
    {
        type = "function";
    }

    return type;
}

std::string PythonEquationContext::GetTypeCategory(const std::string &type_name) const
{
    // Classify Python type names into one of: Function, Module, Class, Variable
    if (type_name == "module")
    {
        return "Module";
    }

    // Check if it's a class/type
    if (type_name == "type")
    {
        return "Class";
    }

    // Common function-like type names
    if (type_name == "function" || type_name == "builtin_function_or_method" || type_name == "method" ||
        type_name == "builtin_method" || type_name == "staticmethod" || type_name == "classmethod")
    {
        return "Function";
    }

    // Everything else treated as a variable (includes int, float, str, custom classes, etc.)
    return "Variable";
}