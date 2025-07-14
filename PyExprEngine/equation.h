#include <pybind11/detail/common.h>
#include <set>
#include <string>
#include <pybind11/pybind11.h>

namespace py = pybind11;

class Equation {
public:
    enum ErrorType {
        NO_ERROR,
        SYNTAX_ERROR,
        NAME_ERROR,
        KEY_ERROR,
        TYPE_ERROR,
        VALUE_ERROR,
    };
    Equation(const std::string &name, const std::string &expression)
        : name_(name), expression_(expression) { Evaluate(); }
    bool is_valid() const {
        return is_valid_;
    }

    ErrorType error_type() const {
        return error_type_;
    }

    const std::string& error_message() const {
        return error_message_;
    }

    const std::string& name() const {
        return name_;
    }

    const std::string& expression() const {
        return expression_;
    }

    const py::object& result() const {
        return result_;
    }
    
    bool Evaluate();
private:
    std::string name_;
    std::string expression_;
    py::object result_;

    // error handling
    bool is_valid_ = false;
    ErrorType error_type_ = NO_ERROR;
    std::string error_message_;
    
    // A depends on B means B is required by A
    std::set<Equation*> depends_on_;
    std::set<Equation*> required_by_;
};