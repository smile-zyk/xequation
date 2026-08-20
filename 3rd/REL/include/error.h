#pragma once

#include "rel_api.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace rel {

// =========================================================================
//  ErrorKind -- categorises where the error originates
// =========================================================================

enum class ErrorKind
{
    Lexical,  // scanner-level (INVALID token)
    Syntax,   // parser-level (grammar mismatch)
    RunTime,  // evaluator-level (type error, unit mismatch, ...)
};

// =========================================================================
// =========================================================================
//  Error -- unified error with source location
// =========================================================================

struct REL_API Error
{
    ErrorKind  kind;
    int        line;
    int        column;
    std::string message;

    /// e.g. "syntax error: line 1, column 4: unexpected token after expression; found `Z`"
    std::string to_string() const;
};

// =========================================================================
//  RelError -- exception carrying a structured Error
// =========================================================================
//
//  Thrown by the public front-end API (rel::Parse / rel::Eval / rel::Exec).
//  Derives from std::runtime_error so plain `catch (const std::exception&)`
//  keeps working and `what()` returns the same formatted message; callers
//  that need the location / kind can catch rel::RelError and inspect
//  error() (line / column / kind / message).

class REL_API RelError : public std::runtime_error
{
public:
    explicit RelError(Error err)
        : std::runtime_error(err.to_string())
        , error_(std::move(err))
    {}

    /// The structured error (kind + line + column + message).
    const Error& error() const { return error_; }

    /// The formatted message, e.g.
    /// "syntax error: line 1, column 4: unexpected token after expression; found `Z`".
    /// Equivalent to what(); mirrors Error::to_string().
    std::string to_string() const { return error_.to_string(); }

private:
    Error error_;
};

} // namespace rel
