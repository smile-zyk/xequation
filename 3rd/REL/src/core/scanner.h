#pragma once

#include "error.h"
#include "token.h"

#include <cstddef>
#include <string>
#include <vector>

namespace rel
{
    // ========================================================================
    // ScanResult — scanner output
    // ========================================================================

    struct ScanResult
    {
        std::vector<Token> tokens;      // up to the first error, or all
        std::vector<Error> errors;      // lexical errors encountered

        bool Ok() const { return errors.empty(); }
    };

    // ========================================================================
    // Scanner — hand-written single-pass lexer
    // ========================================================================
    //
    // Turns a string of source text into tokens defined in
    // REL_Formal_Spec.md sections 1 and 2.
    //
    // Usage:
    //   Scanner s(line, line_no);
    //   ScanResult result = s.scan();
    //
    // The scanner is one-shot: construct, call scan() once, then discard.
    // The returned tokens always end with one END_OF_INPUT token.
    // Lexical errors are collected in ScanResult::errors (no INVALID tokens).

    class Scanner
    {
    public:
        explicit Scanner(std::string source, int initial_line = 1);

        ScanResult Scan();

    private:
        // --- Driver ------------------------------------------------------
        void scan_one_token();
        void mark_token_start();

        // --- Cursor helpers ---------------------------------------------
        bool is_at_end() const;
        char peek(std::size_t offset = 0) const;
        char advance();
        bool match(char expected);

        // --- Token emission ---------------------------------------------
        void emit(TokenType type);
        void emit(TokenType type, std::string lexeme);

        // --- Error collection -------------------------------------------
        void add_error(const std::string& message);

        // --- Sub-scanners -----------------------------------------------
        void scan_identifier_or_keyword();
        void scan_string_literal();
        void scan_raw_string_literal();
        void scan_numeric_literal();
        void scan_numeric_base();
        void scan_numeric_suffix();

        // --- State ------------------------------------------------------
        std::string source_;
        std::vector<Token> tokens_;
        std::vector<Error> errors_;
        std::size_t start_ = 0;       // start offset of the current token
        std::size_t current_ = 0;     // next char to consume
        std::size_t line_start_ = 0;  // offset of the current line's first char
        int line_ = 1;
        int start_line_ = 1;
        int start_col_ = 1;
    };
} // namespace rel
