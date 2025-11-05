#pragma once
#include <string>
#include <vector>


namespace xequation
{
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

    void set_type(Type type)
    {
        type_ = type;
    }

    Type type() const
    {
        return type_;
    }

    void set_status(Status status)
    {
        status_ = status;
    }

    Status status() const
    {
        return status_;
    }

    void set_message(const std::string &message)
    {
        message_ = message;
    }

    const std::string &message()
    {
        return message_;
    }

    bool operator==(const Equation &other) const
    {
        return name_ == other.name_ && content_ == other.content_ && dependencies_ == other.dependencies_ &&
               type_ == other.type_ && status_ == other.status_ && message_ == other.message_;
    }

    bool operator!=(const Equation &other) const
    {
        return !(*this == other);
    }

  private:
    std::string name_;
    std::string content_;
    std::vector<std::string> dependencies_;
    Type type_;
    Status status_;
    std::string message_;
};
} // namespace xequation

namespace std
{
template <>
struct hash<xequation::Equation>
{
    std::size_t operator()(const xequation::Equation &eq) const noexcept
    {
        std::size_t h1 = std::hash<std::string>{}(eq.name());
        std::size_t h2 = std::hash<std::string>{}(eq.content());
        std::size_t h3 = std::hash<int>{}(static_cast<int>(eq.type()));
        std::size_t h4 = std::hash<int>{}(static_cast<int>(eq.status()));

        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};
} // namespace std