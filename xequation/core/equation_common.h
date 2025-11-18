#pragma once

#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "value.h"
#include "equation.h"

namespace xequation
{
class EquationContext;

struct ExecResult
{
    Equation::Status status;
    std::string message;
};

struct EvalResult
{
    Value value;
    Equation::Status status;
    std::string message;
};

struct ParseResultItem
{
    std::string name;
    std::string content;
    Equation::Type type;
    std::vector<std::string> dependencies;

    bool operator==(const ParseResultItem &other) const
    {
        return name == other.name && content == other.content && type == other.type &&
               dependencies == other.dependencies;
    }

    bool operator!=(const ParseResultItem &other) const
    {
        return !(*this == other);
    }
};

using ParseResult = std::vector<ParseResultItem>;

class ParseException : public std::exception
{
  private:
    std::string error_message_;

  public:
    ParseException(const std::string &message) : error_message_(message) {}

    const char *what() const noexcept override
    {
        return error_message_.c_str();
    }

    const std::string &error_message() const
    {
        return error_message_;
    }
};

using ExecHandler = std::function<ExecResult(const std::string &, EquationContext *)>;
using EvalHandler = std::function<EvalResult(const std::string &, EquationContext *)>;
using ParseHandler = std::function<ParseResult(const std::string &)>;
} // namespace xequation

namespace std
{
template <>
struct hash<xequation::ParseResultItem>
{
    size_t operator()(const xequation::ParseResultItem &item) const
    {
        size_t h = 0;
        std::hash<std::string> string_hasher;
        std::hash<xequation::Equation::Type> type_hasher;

        h ^= string_hasher(item.name) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= string_hasher(item.content) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= type_hasher(item.type) + 0x9e3779b9 + (h << 6) + (h >> 2);

        for (const auto &dep : item.dependencies)
        {
            h ^= string_hasher(dep) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }
};
} // namespace std