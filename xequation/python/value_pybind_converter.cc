#include "value_pybind_converter.h"

std::ostream &operator<<(std::ostream &os, const pybind11::object &obj)
{
    using namespace pybind11::literals;
    std::ostringstream oss;

    pybind11::print(obj, "file"_a = &oss);
    std::string content = oss.str();

    if (!content.empty() && content.back() == '\n')
    {
        content.pop_back();
    }

    try
    {
        auto type_name = obj.attr("__class__").attr("__name__").cast<std::string>();
        os << "<" << type_name << "> " << content;
    }
    catch (const pybind11::error_already_set &e)
    {
        os << "<unknown> " << content;
    }

    return os;
}