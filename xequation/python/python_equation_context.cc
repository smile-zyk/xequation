#include "python_equation_context.h"

using namespace xequation;
using namespace xequation::python;

PythonEquationContext::PythonEquationContext()
{
    pybind11::object builtins = pybind11::module_::import("builtins");
    dict_["__builtins__"] = builtins;
}

Value PythonEquationContext::Get(const std::string &var_name) const
{
    pybind11::gil_scoped_acquire acquire;
    if (dict_.contains(var_name))
    {
        return pybind11::cast<Value>(dict_[var_name.c_str()]);
    }
    return Value::Null();
}

bool PythonEquationContext::Contains(const std::string &var_name) const
{
    pybind11::gil_scoped_acquire acquire;
    return dict_.contains(var_name);
}

std::unordered_set<std::string> PythonEquationContext::keys() const
{
    pybind11::gil_scoped_acquire acquire;

    pybind11::object keys_obj = dict_.attr("keys")();
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

    dict_[var_name.c_str()] = value;
}

bool PythonEquationContext::Remove(const std::string &var_name)
{
    pybind11::gil_scoped_acquire acquire;

    if (dict_.contains(var_name))
    {
        dict_.attr("__delitem__")(var_name);
        return true;
    }
    return false;
}

void PythonEquationContext::Clear() 
{
    pybind11::gil_scoped_acquire acquire;
    
    dict_.clear();
}

size_t PythonEquationContext::size() const
{
    pybind11::gil_scoped_acquire acquire;
    
    return dict_.size();
}

bool PythonEquationContext::empty() const
{
    pybind11::gil_scoped_acquire acquire;

    return dict_.empty();
}