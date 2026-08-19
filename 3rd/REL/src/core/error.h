#pragma once

#include <string>

namespace rel {

// =========================================================================
//  ErrorKind — categorises where the error originates
// =========================================================================

enum class ErrorKind
{
    Lexical,  // scanner-level (INVALID token)
    Syntax,   // parser-level (grammar mismatch)
    RunTime,  // evaluator-level (type error, unit mismatch, ...)
};

// =========================================================================
// =========================================================================
//  Error — unified error with source location
// =========================================================================

struct Error
{
    ErrorKind  kind;
    int        line;
    int        column;
    std::string message;

    /// e.g. "syntax error: line 1, column 4: unexpected token after expression; found `Z`"
    std::string to_string() const;
};

} // namespace rel
