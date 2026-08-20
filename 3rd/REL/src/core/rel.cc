#include "rel.h"

#include "environment.h"
#include "error.h"
#include "evaluator.h"
#include "parser.h"
#include "scanner.h"

#include <cctype>
#include <string>
#include <utility>

namespace rel {
namespace {

/// Build a RelError from scanner / parser output (kind Lexical or Syntax).
RelError make_parse_error(ErrorKind kind, const Error& first_error)
{
    Error err;
    err.kind    = kind;
    err.line    = first_error.line;
    err.column  = first_error.column;
    err.message = first_error.message;
    return RelError(err);
}

/// Wrap a thrown std::exception raised during evaluation as a RunTime
/// RelError anchored at the expression's source location.
RelError make_eval_error(const Expr& expr, const std::exception& e)
{
    Error err;
    err.kind    = ErrorKind::RunTime;
    err.line    = expr.line;
    err.column  = expr.column;
    err.message = e.what();
    return RelError(err);
}

bool is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_char(char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

bool is_valid_identifier(const std::string& name)
{
    if (name.empty() || !is_ident_start(name[0]))
        return false;
    for (std::size_t i = 1; i < name.size(); ++i)
        if (!is_ident_char(name[i]))
            return false;
    if (name == "if" || name == "then" || name == "elseif" || name == "else" ||
        name == "AND" || name == "OR" || name == "NOT" ||
        name == "EQUALS" || name == "NOTEQUALS")
        return false;
    return true;
}

std::string trim(const std::string& s)
{
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

/// Find the top-level binding '=' of a `name = expr` line.  Skips the
/// comparison/assignment operators (==, !=, <=, >=, =), returning npos when
/// the line is a plain expression.
std::size_t find_binding_eq(const std::string& line)
{
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] != '=') continue;
        if (i > 0)
        {
            char prev = line[i - 1];
            if (prev == '=' || prev == '!' || prev == '<' || prev == '>')
                continue;
        }
        if (i + 1 < line.size() && line[i + 1] == '=')
            continue;
        return i;
    }
    return std::string::npos;
}

}  // namespace

ExprPtr Parse(const std::string& source)
{
    Scanner scanner(source);
    ScanResult sr = scanner.Scan();
    if (!sr.Ok())
        throw make_parse_error(ErrorKind::Lexical, sr.errors[0]);

    Parser parser(std::move(sr.tokens));
    ParseResult result = parser.Parse();
    if (!result.Ok())
        throw make_parse_error(ErrorKind::Syntax, result.errors[0]);

    return std::move(result.expr);
}

Value Eval(const std::string& source, Environment* env)
{
    Environment temp_env;
    if (!env) env = &temp_env;

    ExprPtr expr = Parse(source);
    try
    {
        return Eval(*expr, *env);
    }
    catch (const RelError&)
    {
        throw;  // already structured
    }
    catch (const std::exception& e)
    {
        throw make_eval_error(*expr, e);
    }
}

Value Eval(const Expr& expr, Environment& env)
{
    try
    {
        Evaluator evaluator(env);
        return evaluator.Evaluate(expr);
    }
    catch (const RelError&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw make_eval_error(expr, e);
    }
}

void Exec(const std::string& source, Environment& env)
{
    std::size_t eq = find_binding_eq(source);
    if (eq == std::string::npos)
    {
        // Plain expression: evaluate and discard the result.
        Eval(source, &env);
        return;
    }

    std::string name = trim(source.substr(0, eq));
    std::string expr_str = trim(source.substr(eq + 1));
    if (!is_valid_identifier(name))
    {
        Error err;
        err.kind    = ErrorKind::Syntax;
        err.line    = 1;
        err.column  = 1;
        err.message = "invalid identifier '" + name + "'";
        throw RelError(err);
    }

    Value v = Eval(expr_str, &env);
    env.Define(name, v);
}

} // namespace rel
