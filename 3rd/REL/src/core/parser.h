#pragma once

#include "rel_api.h"
#include "expr.h"
#include "error.h"
#include "token.h"

#include <string>
#include <vector>

namespace rel
{
    struct REL_API ParseResult
    {
        ExprPtr expr;
        std::vector<Error> errors;

        bool Ok() const;
    };

    class REL_API Parser
    {
    public:
        explicit Parser(std::vector<Token> tokens);

        ParseResult Parse();

    private:
        struct ArgList
        {
            std::vector<ExprPtr> args;
            bool valid;
        };

        ExprPtr expression();
        ExprPtr conditional();
        ExprPtr logical_or();
        ExprPtr logical_and();
        ExprPtr bit_or();
        ExprPtr bit_xor();
        ExprPtr bit_and();
        ExprPtr equality();
        ExprPtr comparison();
        ExprPtr shift();
        ExprPtr term();
        ExprPtr factor();
        ExprPtr unary();
        ExprPtr power();
        ExprPtr power_rhs();
        ExprPtr postfix();
        ExprPtr primary();
        ExprPtr if_expression();
        ExprPtr literal();
        ExprPtr reference();
        ExprPtr sweep_generator();
        ExprPtr matrix_generator();

        std::vector<ExprPtr> item_list(bool allow_null_range);
        std::vector<ExprPtr> index_list();
        ArgList call_arg_list();
        ExprPtr sequence(bool allow_null_range);

        bool match(TokenType type);
        bool match_any(TokenType a, TokenType b);
        const Token& consume(TokenType type, const char* message);
        bool check(TokenType type) const;
        const Token& advance();
        bool is_at_end() const;
        const Token& peek() const;
        const Token& previous() const;

        void report_here(const char* message);
        void report_at(const Token& token, const std::string& message);

        ExprPtr make_binary(ExprPtr left, const Token& op, ExprPtr right);
        ExprPtr make_logical(ExprPtr left, const Token& op, ExprPtr right);

        static NumberKind classify_number_kind(const std::string& lexeme);
        static int classify_radix(const std::string& lexeme, NumberKind kind);
        static bool decode_string_literal(const std::string& lexeme, std::string* out);
        static std::string strip_raw_string_quotes(const std::string& lexeme);

        std::vector<Token> tokens_;
        std::size_t current_;
        std::vector<Error> errors_;
    };
} // namespace rel
