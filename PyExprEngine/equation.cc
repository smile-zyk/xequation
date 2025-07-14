#include "equation.h"
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

bool Equation::Evaluate()
{
    try {
        if (expression_.empty()) {
            throw std::runtime_error("Expression cannot be empty");
        }
        result_ = pybind11::eval(expression_.c_str());
        if (pybind11::isinstance<pybind11::none>(result_)) {
            throw std::runtime_error("Expression evaluated to None");
        }
        pybind11::globals()[name_.c_str()] = result_;
        is_valid_ = true;
    } catch (const std::exception& e) {
        error_message_ = e.what();
        if (std::string(e.what()).find("Syntax") != std::string::npos) {
            error_type_ = SYNTAX_ERROR;
        } else if (std::string(e.what()).find("Name") != std::string::npos) {
            error_type_ = NAME_ERROR;
        } else if (std::string(e.what()).find("Key") != std::string::npos) {
            error_type_ = KEY_ERROR;
        } else if (std::string(e.what()).find("Type") != std::string::npos) {
            error_type_ = TYPE_ERROR;
        } else {
            error_type_ = VALUE_ERROR;
        }
        is_valid_ = false;
    }
    return is_valid_;
}