#include "py_expr_context.h"

using namespace xequation;
using namespace xequation::python;

Value PyExprContext::Get(const std::string &var_name) const
{
    py::gil_scoped_acquire acquire;
    if (dict_.contains(var_name))
    {
        return py::cast<Value>(dict_[var_name.c_str()]);
    }
    return Value::Null();
}

bool PyExprContext::Contains(const std::string &var_name) const
{
    py::gil_scoped_acquire acquire;
    return dict_.contains(var_name);
}

std::unordered_set<std::string> PyExprContext::keys() const
{
    py::gil_scoped_acquire acquire;

    py::object keys_obj = dict_.attr("keys")();
    std::unordered_set<std::string> keys;

    for (auto key : keys_obj)
    {
        keys.insert(key.cast<std::string>());
    }
    return keys;
}

void PyExprContext::Set(const std::string &var_name, const Value &value)
{
    py::gil_scoped_acquire acquire;

    dict_[var_name.c_str()] = value;
}

bool PyExprContext::Remove(const std::string &var_name)
{
    py::gil_scoped_acquire acquire;

    if (dict_.contains(var_name))
    {
        dict_.attr("__delitem__")(var_name);
        return true;
    }
    return false;
}

void PyExprContext::Clear() 
{
    py::gil_scoped_acquire acquire;
    
    dict_.clear();
}

size_t PyExprContext::size() const
{
    py::gil_scoped_acquire acquire;
    
    return dict_.size();
}

bool PyExprContext::empty() const
{
    py::gil_scoped_acquire acquire;

    return dict_.empty();
}