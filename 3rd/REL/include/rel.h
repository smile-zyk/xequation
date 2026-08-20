#pragma once

#include "rel_api.h"
#include "expr.h"       // rel::Expr / ExprPtr (syntax tree)
#include "function.h"   // FunctionParam / NativeFunction
#include "value.h"      // rel::Value

#include <string>

namespace rel {

class Environment;

/// Parse a single REL expression from a source string into a syntax tree.
/// Returns the parsed AST (ExprPtr); throws std::runtime_error with the
/// formatted error ("syntax error: line 1, column 4: ...") on failure.
/// The returned tree can be evaluated with Eval(const Expr&, Environment&)
/// without re-parsing, or inspected/transformed by an ExprVisitor.
REL_API ExprPtr Parse(const std::string& source);

/// Parse and evaluate a single REL expression from a source string.
/// When `env` is nullptr (the default), a temporary Environment is used.
/// Otherwise the given Environment is used (with its variables, datasets,
/// and built-in constants).
/// Throws std::runtime_error on parse or evaluation failure.
REL_API Value Eval(const std::string& source, Environment* env = nullptr);

/// Evaluate an already-parsed syntax tree against an Environment.
/// Throws std::runtime_error on evaluation failure.
REL_API Value Eval(const Expr& expr, Environment& env);

/// Execute a single line of REL source, which may be either a plain
/// expression or a `name = expr` binding:
///   - plain expression: parsed and evaluated; the result is discarded;
///   - `name = expr`:    the expression is evaluated and bound in `env`
///     as a variable (throws std::runtime_error when `name` is not a
///     valid identifier).
/// Throws std::runtime_error on parse, evaluation, or binding failure.
REL_API void Exec(const std::string& source, Environment& env);

} // namespace rel
