#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <exception>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include <tsl/ordered_map.h>

#include "bitmask.hpp"
#include "equation_value.h"

namespace xequation
{
// Identity tags are a UI / host concern: the core stores whatever `tag`
// string a caller passes (empty by default) without interpreting it.  Hosts
// that need default tags / tag groups define them on their own layer (see
// demo/tree_view_tag.h).

// Coarse-grained result status. Fine-grained errors (e.g. undefined variable,
// division by zero) are carried in InterpretResult::message / Equation::message.
enum class ResultStatus
{
    kPending,
    kCalculating,
    kSuccess,
    kError
};

struct InterpretResult
{
    ResultStatus status;
    std::string message;
    EquationValue value;
};

using ObjectId = boost::uuids::uuid;

/// The nil ObjectId (all bytes zero).  Prefer this over a default-constructed
/// uuid when a nil sentinel is meant: boost::uuids::uuid's default constructor
/// is NOT guaranteed to produce a nil uuid on every compiler / Boost release
/// (older Boost on MSVC left the bytes uninitialized for default-init like
/// `ObjectId x;`).  This goes through boost::uuids::nil_uuid() explicitly.
inline ObjectId NilObjectId()
{
    return boost::uuids::nil_uuid();
}

// An equation binds a name in the environment to an expression.
struct Equation
{
    ObjectId id = NilObjectId();
    std::string name;
    std::string content;
    ResultStatus status = ResultStatus::kPending;
    std::string message;
    /// Static symbols referenced by `content` (extracted at parse time).  This
    /// is a parse snapshot -- the live dependency edges live in the graph.
    std::vector<std::string> parse_symbols;
    std::string tag;   // opaque; empty unless the caller supplies one
};

using EquationPtr = std::unique_ptr<Equation>;
using EquationPtrOrderedMap = tsl::ordered_map<std::string, EquationPtr>;

class EquationException : public std::exception
{
  public:
    enum class ErrorCode
    {
        kEquationNotFound,
        kEquationAlreadyExists,
        kExpressionNotFound,
    };

    const char *what() const noexcept override
    {
        if (message_cache_.empty())
        {
            message_cache_ = GenerateErrorMessage();
        }
        return message_cache_.c_str();
    }

    const std::string &equation_name() const
    {
        return equation_name_;
    }

    const ObjectId &id() const
    {
        return id_;
    }

    ErrorCode error_code() const
    {
        return error_code_;
    }

    static EquationException EquationNotFound(const std::string &equation_name)
    {
        return EquationException(ErrorCode::kEquationNotFound, equation_name);
    }

    static EquationException EquationNotFound(const ObjectId &id)
    {
        return EquationException(ErrorCode::kEquationNotFound, id);
    }

    static EquationException EquationAlreadyExists(const std::string &equation_name)
    {
        return EquationException(ErrorCode::kEquationAlreadyExists, equation_name);
    }

    static EquationException ExpressionNotFound(const std::string &expression_id)
    {
        return EquationException(ErrorCode::kExpressionNotFound, expression_id);
    }

  private:
    std::string GenerateErrorMessage() const
    {
        std::ostringstream oss;

        switch (error_code_)
        {
        case ErrorCode::kEquationNotFound:
            if (!id_.is_nil())
            {
                oss << "Equation not found. ID: '" << boost::uuids::to_string(id_) << "'";
            }
            else
            {
                oss << "Equation not found. Name: '" << equation_name_ << "'";
            }
            break;

        case ErrorCode::kEquationAlreadyExists:
            oss << "Equation already exists. Name: '" << equation_name_ << "'";
            break;

        case ErrorCode::kExpressionNotFound:
            oss << "Expression not found. ID: '" << equation_name_ << "'";
            break;

        default:
            oss << "Unknown equation error occurred.";
            break;
        }

        return oss.str();
    }

    EquationException(ErrorCode error_code, const std::string &equation_name)
        : error_code_(error_code), equation_name_(equation_name)
    {
    }

    EquationException(ErrorCode error_code, const ObjectId &id)
        : error_code_(error_code), id_(id)
    {
    }

    ErrorCode error_code_;
    std::string equation_name_;
    ObjectId id_;
    mutable std::string message_cache_;
};

// A registered expression: observe-only computation. It participates in the
// dependency graph (so it recomputes when its inputs change) but is never
// written into the environment.
struct Expression
{
    ObjectId id = NilObjectId();
    std::string content;
    InterpretResult result;
    /// Static symbols referenced by `content` (extracted at parse time).  This
    /// is a parse snapshot -- the live dependency edges live in the graph.
    std::vector<std::string> parse_symbols;
    std::string tag;   // opaque; empty unless the caller supplies one
};

// Result of parsing a single expression: syntax check + extracted symbols.
struct ParseResult
{
    ResultStatus status = ResultStatus::kSuccess;
    std::string message;
    std::vector<std::string> symbols;
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

enum class EquationUpdateFlag
{
    kName = 1 << 0,
    kContent = 1 << 1,
    kStatus = 1 << 2,
    kMessage = 1 << 3,
    kValue = 1 << 4,
    kDependencies = 1 << 5,
    kDependents = 1 << 6,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationUpdateFlag, kDependents)

enum class ExpressionUpdateFlag
{
    kContent = 1 << 0,
    kStatus = 1 << 1,
    kMessage = 1 << 2,
    kValue = 1 << 3,
    kDependencies = 1 << 4,
};

BITMASK_DEFINE_MAX_ELEMENT(ExpressionUpdateFlag, kDependencies)

class ResultStatusConverter
{
  public:
    static ResultStatus FromString(const std::string &status_str)
    {
        if (status_str == "Pending")
            return ResultStatus::kPending;
        else if (status_str == "Calculating")
            return ResultStatus::kCalculating;
        else if (status_str == "Success")
            return ResultStatus::kSuccess;
        else
            return ResultStatus::kError;
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
        default:
            return "Error";
        }
    }
};

inline std::ostream &operator<<(std::ostream &os, ResultStatus status)
{
    return os << ResultStatusConverter::ToString(status);
}
} // namespace xequation
