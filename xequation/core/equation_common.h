#pragma once

#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "bitmask.hpp"
#include "value.h"

namespace xequation
{
class EquationContext;

enum class ResultStatus
{
    kPending,
    kCalculating,
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
    kUnknownError
};

enum class InterpretMode
{
    kExec,
    kEval
};

struct InterpretResult
{
    InterpretMode mode;
    ResultStatus status;
    std::string message;
    Value value;
};

enum class ItemType
{
    kUnknown,
    kExpression,
    kVariable,
    kFunction,
    kClass,
    kImport,
    kImportFrom,
    kError,
};

struct ParseResultItem
{
    std::string name;
    std::string content;
    ItemType type;
    std::vector<std::string> dependencies;
    std::string message;
    ResultStatus status;
    bool operator==(const ParseResultItem &other) const
    {
        return name == other.name && content == other.content && type == other.type && dependencies == other.dependencies;
    }

    bool operator!=(const ParseResultItem &other) const
    {
        return !(*this == other);
    }
};

enum class ParseMode
{
    kStatement,
    kExpression,
};

struct ParseResult
{
    ParseMode mode;
    std::vector<ParseResultItem> items;
};

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

class ItemTypeConverter
{
  public:
    static ItemType FromString(const std::string &type_str)
    {
        if (type_str == "Expression")
            return ItemType::kExpression;
        else if (type_str == "Variable")
            return ItemType::kVariable;
        else if (type_str == "Function")
            return ItemType::kFunction;
        else if (type_str == "Class")
            return ItemType::kClass;
        else if (type_str == "Import")
            return ItemType::kImport;
        else if (type_str == "ImportFrom")
            return ItemType::kImportFrom;
        else if (type_str == "Error")
            return ItemType::kError;
        else
            return ItemType::kUnknown;
    }

    static std::string ToString(ItemType type)
    {
        switch (type)
        {
        case ItemType::kExpression:
            return "Expression";
        case ItemType::kVariable:
            return "Variable";
        case ItemType::kFunction:
            return "Function";
        case ItemType::kClass:
            return "Class";
        case ItemType::kImport:
            return "Import";
        case ItemType::kImportFrom:
            return "ImportFrom";
        case ItemType::kError:
            return "Error";
        default:
            return "Unknown";
        }
    }
}; 

inline std::ostream &operator<<(std::ostream &os, ItemType type)
{
    return os << ItemTypeConverter::ToString(type);
}

enum class EquationUpdateFlag
{
    kContent = 1 << 0,
    kType = 1 << 1,
    kStatus = 1 << 2,
    kMessage = 1 << 3,
    kValue = 1 << 5,
    kDependencies = 1 << 6,
    kDependents = 1 << 7,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationUpdateFlag, kDependents)

enum class EquationGroupUpdateFlag
{
    kStatement = 1 << 0,
    kEquationCount = 1 << 1,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationGroupUpdateFlag, kEquationCount)

class ResultStatusConverter
{
  public:
    static ResultStatus FromString(const std::string &status_str)
    {
        if (status_str == "Pending")
            return ResultStatus::kPending;
        else if( status_str == "Calculating")
            return ResultStatus::kCalculating;
        else if (status_str == "Success")
            return ResultStatus::kSuccess;
        else if (status_str == "SyntaxError")
            return ResultStatus::kSyntaxError;
        else if (status_str == "NameError")
            return ResultStatus::kNameError;
        else if (status_str == "TypeError")
            return ResultStatus::kTypeError;
        else if (status_str == "ZeroDivisionError")
            return ResultStatus::kZeroDivisionError;
        else if (status_str == "ValueError")
            return ResultStatus::kValueError;
        else if (status_str == "MemoryError")
            return ResultStatus::kMemoryError;
        else if (status_str == "OverflowError")
            return ResultStatus::kOverflowError;
        else if (status_str == "RecursionError")
            return ResultStatus::kRecursionError;
        else if (status_str == "IndexError")
            return ResultStatus::kIndexError;
        else if (status_str == "KeyError")
            return ResultStatus::kKeyError;
        else if (status_str == "AttributeError")
            return ResultStatus::kAttributeError;
        else
            return ResultStatus::kPending;
    }

    static std::string ToString(ResultStatus status)
    {
        switch (status)
        {
        case ResultStatus::kPending:
            return "Pending";
        case ResultStatus::kCalculating:
            return "Calculating";
        case ResultStatus::kSuccess:
            return "Success";
        case ResultStatus::kSyntaxError:
            return "SyntaxError";
        case ResultStatus::kNameError:
            return "NameError";
        case ResultStatus::kTypeError:
            return "TypeError";
        case ResultStatus::kZeroDivisionError:
            return "ZeroDivisionError";
        case ResultStatus::kValueError:
            return "ValueError";
        case ResultStatus::kMemoryError:
            return "MemoryError";
        case ResultStatus::kOverflowError:
            return "OverflowError";
        case ResultStatus::kRecursionError:
            return "RecursionError";
        case ResultStatus::kIndexError:
            return "IndexError";
        case ResultStatus::kKeyError:
            return "KeyError";
        case ResultStatus::kAttributeError:
            return "AttributeError";
        default:
            return "Unknown";
        }
    }
};

inline std::ostream &operator<<(std::ostream &os, ResultStatus status)
{
    return os << ResultStatusConverter::ToString(status);
}

using InterpretHandler = std::function<InterpretResult(const std::string &, EquationContext *, InterpretMode)>;
using ParseHandler = std::function<ParseResult(const std::string &, ParseMode)>;
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
        std::hash<xequation::ItemType> type_hasher;
        std::hash<xequation::ResultStatus> status_hasher;

        h ^= string_hasher(item.name) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= string_hasher(item.content) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= type_hasher(item.type) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= string_hasher(item.message) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= status_hasher(item.status) + 0x9e3779b9 + (h << 6) + (h >> 2);

        for (const auto &dep : item.dependencies)
        {
            h ^= string_hasher(dep) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }
};
} // namespace std