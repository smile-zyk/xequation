#include "value_pybind_converter.h"

std::ostream& operator<<(std::ostream& os, const pybind11::object& obj)
{
    using namespace pybind11::literals;
    std::ostringstream oss;
    
    pybind11::print(obj, "file"_a = &oss);
    os << oss.str();
    
    return os;
}