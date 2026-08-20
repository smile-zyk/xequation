#pragma once

#include "rel_api.h"

#include <string>

namespace rel
{
    // Token kinds correspond one-to-one with the terminals defined in
    // REL_Formal_Spec.md sections 1 and 2.
    enum class TokenType
    {
        // -----------------------------------------------------------------
        // Keywords (Spec 1.1)
        // Recognised after IDENTIFIER maximal-munch matching; see Spec 1.7
        // rule 4. Built-in constants such as `PI` / `e` / `ln10` are NOT
        // keywords - they are ordinary identifiers resolved at evaluation.
        // -----------------------------------------------------------------
        KW_IF,        // if
        KW_THEN,      // then
        KW_ELSEIF,    // elseif
        KW_ELSE,      // else
        KW_AND,       // AND        (alias of &&, see Spec 2.3)
        KW_OR,        // OR         (alias of ||, see Spec 2.3)
        KW_NOT,       // NOT        (alias of !,  see Spec 2.5 unary_op)
        KW_EQUALS,    // EQUALS     (alias of ==, see Spec 2.4)
        KW_NOTEQUALS, // NOTEQUALS  (alias of !=, see Spec 2.4)
        KW_TRUE,      // TRUE       boolean literal
        KW_FALSE,     // FALSE      boolean literal

        // -----------------------------------------------------------------
        // Identifier (Spec 1.1)
        //   IDENTIFIER = [A-Za-z_][A-Za-z0-9_]*
        // Used by <reference> in Spec 2.7.
        // -----------------------------------------------------------------
        IDENTIFIER,

        // -----------------------------------------------------------------
        // Operators (Spec 1.2)
        // Multi-character operators win by maximal munch (Spec 1.7 rule 1):
        //   **  ::  <<  >>  >=  <=  ==  !=  &&  ||
        // take precedence over their single-character prefixes.
        // -----------------------------------------------------------------
        OP_POW,   // **
        OP_SEQ,   // ::    sequence/range constructor, see Spec 2.6 seq_expr
        OP_SHL,   // <<
        OP_SHR,   // >>
        OP_GE,    // >=
        OP_LE,    // <=
        OP_EQ,    // ==
        OP_NE,    // !=
        OP_LAND,  // &&
        OP_LOR,   // ||
        OP_LT,    // <
        OP_GT,    // >
        OP_ADD,   // +
        OP_SUB,   // -     also unary minus, see Spec 2.5 unary_op
        OP_MUL,   // *
        OP_DIV,   // /
        OP_MOD,   // %
        OP_BXOR,  // ^
        OP_BOR,   // |
        OP_BAND,  // &
        OP_BNOT,  // ~
        OP_LNOT,  // !
        OP_QMARK, // ?     ternary, see Spec 2.2 conditional_tail
        OP_COLON, // :     ternary, see Spec 2.2 conditional_tail

        // -----------------------------------------------------------------
        // Delimiters (Spec 1.2)
        // DDOT (..) wins over DOT (.) by maximal munch; DDOT is used by
        // node-reference syntax in <reference_tail> (Spec 2.7).
        // -----------------------------------------------------------------
        LPAREN,   // (
        RPAREN,   // )
        LBRACKET, // [     sweep generator, see Spec 2.6
        RBRACKET, // ]
        LBRACE,   // {     matrix generator, see Spec 2.6
        RBRACE,   // }
        COMMA,    // ,
        DOT,      // .     member access in <reference_tail> (Spec 2.7)
        DDOT,     // ..    cross-dataset reference (Spec 2.7)

        // -----------------------------------------------------------------
        // String literals (Spec 1.3)
        //   STRING_LITERAL     = "..."   with C-like escapes
        //   RAW_STRING_LITERAL = ''...'' verbatim, no escapes
        // -----------------------------------------------------------------
        STRING_LITERAL,
        RAW_STRING_LITERAL,

        // -----------------------------------------------------------------
        // Numeric literal suffix (Spec 1.5)
        // NUMERIC_SUFFIX is emitted as a single token.  The scanner
        // greedily matches the longest valid suffix using the priority
        // PREDEF_SCALED_UNIT > SCALE_FACTOR + UNIT > UNIT.
        // -----------------------------------------------------------------
        NUMERIC_BASE,    // 123 / 0x1F / 3.14 / 077 / 2i ...
        NUMERIC_SUFFIX,  // GHz, cm, mV, Hz, Ohm, dB, ...

        // -----------------------------------------------------------------
        // Control tokens (not produced by any regex in Spec 1)
        // -----------------------------------------------------------------
        END_OF_INPUT, // <eof>, input exhausted (Spec 1.7 rule 5)
        INVALID,      // fallback for lexical errors
    };

    struct Token
    {
        TokenType type;
        std::string lexeme;
        int line;
        int column;

        Token() : type(TokenType::INVALID), line(0), column(0) {}
        Token(TokenType t, std::string l, int ln, int col)
            : type(t), lexeme(std::move(l)), line(ln), column(col) {}
    };

    REL_API std::string to_string(TokenType type);

    // Render a token as a short fragment that reads naturally when
    // embedded inside an error message.
    //   e.g.  `if` (line 1, column 1)
    //         <end of input> (line 3, column 1)
    REL_API std::string to_string(const Token& token);
} // namespace rel