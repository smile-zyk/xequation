#pragma once
#include "expr_common.h"
#include <string>

namespace xequation
{
class Equation
{
  public:
    Equation() = default;
    Equation(const std::string &name) : name_(name) {}
    virtual ~Equation() = default;

    void set_name(const std::string &name)
    {
        name_ = name;
    }

    const std::string &name() const
    {
        return name_;
    }

    void set_content(const std::string &content)
    {
        content_ = content;
    }

    const std::string &content() const
    {
        return content_;
    }

    void set_dependencies(const std::vector<std::string> &dependencies)
    {
        dependencies_ = dependencies;
    }

    const std::vector<std::string> &dependencies() const
    {
        return dependencies_;
    }

    void set_type(ParseType type)
    {
        type_ = type;
    }

    ParseType type() const
    {
        return type_;
    }

    void set_status(ExecStatus status)
    {
        status_ = status;
    }

    ExecStatus status() const
    {
        return status_;
    }

    void set_message(const std::string& message)
    {
        message_ = message;
    }

    const std::string& message()
    {
        return message_;
    }

  private:
    std::string name_;
    std::string content_;
    std::vector<std::string> dependencies_;
    ParseType type_;
    ExecStatus status_;
    std::string message_;
};
} // namespace xequation