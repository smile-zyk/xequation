#include "token.h"

#include <sstream>

namespace rel
{
    std::string to_string(TokenType type)
    {
        switch (type)
        {
            case TokenType::KW_IF: return "KW_IF";
            case TokenType::KW_THEN: return "KW_THEN";
            case TokenType::KW_ELSEIF: return "KW_ELSEIF";
            case TokenType::KW_ELSE: return "KW_ELSE";
            case TokenType::KW_AND: return "KW_AND";
            case TokenType::KW_OR: return "KW_OR";
            case TokenType::KW_NOT: return "KW_NOT";
            case TokenType::KW_EQUALS: return "KW_EQUALS";
            case TokenType::KW_NOTEQUALS: return "KW_NOTEQUALS";
            case TokenType::KW_TRUE: return "KW_TRUE";
            case TokenType::KW_FALSE: return "KW_FALSE";

            case TokenType::IDENTIFIER: return "IDENTIFIER";

            case TokenType::OP_POW: return "OP_POW";
            case TokenType::OP_SEQ: return "OP_SEQ";
            case TokenType::OP_SHL: return "OP_SHL";
            case TokenType::OP_SHR: return "OP_SHR";
            case TokenType::OP_GE: return "OP_GE";
            case TokenType::OP_LE: return "OP_LE";
            case TokenType::OP_EQ: return "OP_EQ";
            case TokenType::OP_NE: return "OP_NE";
            case TokenType::OP_LAND: return "OP_LAND";
            case TokenType::OP_LOR: return "OP_LOR";
            case TokenType::OP_LT: return "OP_LT";
            case TokenType::OP_GT: return "OP_GT";
            case TokenType::OP_ADD: return "OP_ADD";
            case TokenType::OP_SUB: return "OP_SUB";
            case TokenType::OP_MUL: return "OP_MUL";
            case TokenType::OP_DIV: return "OP_DIV";
            case TokenType::OP_MOD: return "OP_MOD";
            case TokenType::OP_BXOR: return "OP_BXOR";
            case TokenType::OP_BOR: return "OP_BOR";
            case TokenType::OP_BAND: return "OP_BAND";
            case TokenType::OP_BNOT: return "OP_BNOT";
            case TokenType::OP_LNOT: return "OP_LNOT";
            case TokenType::OP_QMARK: return "OP_QMARK";
            case TokenType::OP_COLON: return "OP_COLON";

            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACKET: return "LBRACKET";
            case TokenType::RBRACKET: return "RBRACKET";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::COMMA: return "COMMA";
            case TokenType::DOT: return "DOT";
            case TokenType::DDOT: return "DDOT";

            case TokenType::STRING_LITERAL: return "STRING_LITERAL";
            case TokenType::RAW_STRING_LITERAL: return "RAW_STRING_LITERAL";

            case TokenType::NUMERIC_BASE: return "NUMERIC_BASE";
            case TokenType::NUMERIC_SUFFIX: return "NUMERIC_SUFFIX";

            case TokenType::END_OF_INPUT: return "END_OF_INPUT";
            case TokenType::INVALID: return "INVALID";
        }
        return "UNKNOWN";
    }

    std::string to_string(const Token& token)
    {
        // Compact fragment that reads naturally inside a sentence, e.g.
        //   unexpected `then`
        //   unexpected <end of input>
        std::ostringstream oss;

        switch (token.type)
        {
            case TokenType::END_OF_INPUT: oss << "<end of input>"; break;
            case TokenType::INVALID:
                // Lexical error: show the offending text directly, but fall
                // back to a placeholder if the lexeme is empty.
                if (token.lexeme.empty())
                {
                    oss << "<invalid token>";
                }
                else
                {
                    oss << "invalid token `" << token.lexeme << '`';
                }
                break;
            case TokenType::STRING_LITERAL:
            case TokenType::RAW_STRING_LITERAL:
                // String literals already carry their own quotes; tag them
                // with <string> instead of wrapping in backticks to avoid
                // hard-to-read nested quoting.
                oss << "<string> " << token.lexeme;
                break;
            default: oss << '`' << token.lexeme << '`'; break;
        }

        return oss.str();
    }
} // namespace rel