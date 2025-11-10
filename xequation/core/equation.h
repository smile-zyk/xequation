#pragma once

#include <string>
#include <vector>

#include "value.h"

namespace xequation
{
class EquationManager;

class Equation
{
  public:
    enum class Type
    {
        kError,
        kVariable,
        kFunction,
        kClass,
        kImport,
        kImportFrom,
    };

    enum class Status
    {
        kInit,
        kSuccess,
        kSyntaxError,
        kNameError,
        kTypeError,
        kZeroDivisionError,
        kValueError,
        kMemoryError,
        kOverflowError,
        kRecursionError,
        kIndexError,
        kKeyError,
        kAttributeError,
    };

    Equation() = default;
    Equation(EquationManager* manager) : manager_(manager) {}
    virtual ~Equation() = default;

    void set_name(const std::string &name) { name_ = name; }
    const std::string &name() const { return name_; }

    void set_content(const std::string &content) { content_ = content; }
    const std::string &content() const { return content_; }

    void set_dependencies(const std::vector<std::string> &dependencies) { dependencies_ = dependencies; }
    const std::vector<std::string> &dependencies() const { return dependencies_; }

    void set_type(Type type) { type_ = type; }
    Type type() const { return type_; }

    void set_status(Status status) { status_ = status; }
    Status status() const { return status_; }

    void set_message(const std::string &message) { message_ = message; }
    const std::string &message() const { return message_; }

    Value GetValue();

    bool operator==(const Equation &other) const;
    bool operator!=(const Equation &other) const;

    static Type StringToType(const std::string &type_str);
    static Status StringToStatus(const std::string &status_str);
    static std::string TypeToString(Type type);
    static std::string StatusToString(Status status);

  private:
    std::string name_;
    std::string content_;
    std::vector<std::string> dependencies_;
    Type type_ = Type::kError;
    Status status_ = Status::kInit;
    std::string message_;
    EquationManager* manager_ = nullptr;
};
} // namespace xequation