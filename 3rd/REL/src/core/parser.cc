#include "parser.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace rel
{
    namespace
    {
        bool is_octal_lexeme(const std::string& lexeme)
        {
            if (lexeme.size() <= 1 || lexeme[0] != '0') return false;
            for (std::size_t i = 1; i < lexeme.size(); ++i)
            {
                if (lexeme[i] < '0' || lexeme[i] > '7') return false;
            }
            return true;
        }
    }

    bool ParseResult::Ok() const { return expr.get() != nullptr && errors.empty(); }

    Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {}

    ParseResult Parser::Parse()
    {
        ParseResult result;
        result.expr = expression();

        if (result.expr && !is_at_end())
        {
            report_here("unexpected token after expression");
        }

        result.errors = errors_;
        if (!result.errors.empty())
        {
            result.expr.reset();
        }
        return result;
    }

    ExprPtr Parser::expression() { return conditional(); }

    ExprPtr Parser::conditional()
    {
        ExprPtr expr = logical_or();
        if (!expr) return ExprPtr();

        while (match(TokenType::OP_QMARK))
        {
            const Token qmark = previous();
            ExprPtr then_branch = expression();
            consume(TokenType::OP_COLON, "expected ':' in conditional expression");
            ExprPtr else_branch = logical_or();
            if (!then_branch || !else_branch) return ExprPtr();
            expr.reset(new ConditionalExpr(qmark.line,
                                           qmark.column,
                                           std::move(expr),
                                           std::move(then_branch),
                                           std::move(else_branch)));
        }

        return expr;
    }

    ExprPtr Parser::logical_or()
    {
        ExprPtr expr = logical_and();
        while (expr && match_any(TokenType::OP_LOR, TokenType::KW_OR))
        {
            Token op = previous();
            ExprPtr right = logical_and();
            expr = make_logical(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::logical_and()
    {
        ExprPtr expr = bit_or();
        while (expr && match_any(TokenType::OP_LAND, TokenType::KW_AND))
        {
            Token op = previous();
            ExprPtr right = bit_or();
            expr = make_logical(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::bit_or()
    {
        ExprPtr expr = bit_xor();
        while (expr && match(TokenType::OP_BOR))
        {
            Token op = previous();
            ExprPtr right = bit_xor();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::bit_xor()
    {
        ExprPtr expr = bit_and();
        while (expr && match(TokenType::OP_BXOR))
        {
            Token op = previous();
            ExprPtr right = bit_and();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::bit_and()
    {
        ExprPtr expr = equality();
        while (expr && match(TokenType::OP_BAND))
        {
            Token op = previous();
            ExprPtr right = equality();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::equality()
    {
        ExprPtr expr = comparison();
        while (expr &&
               (match(TokenType::OP_EQ) || match(TokenType::OP_NE) ||
                match(TokenType::KW_EQUALS) || match(TokenType::KW_NOTEQUALS)))
        {
            Token op = previous();
            ExprPtr right = comparison();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::comparison()
    {
        ExprPtr expr = shift();
        while (expr &&
               (match(TokenType::OP_LT) || match(TokenType::OP_LE) ||
                match(TokenType::OP_GT) || match(TokenType::OP_GE)))
        {
            Token op = previous();
            ExprPtr right = shift();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::shift()
    {
        ExprPtr expr = term();
        while (expr && (match(TokenType::OP_SHL) || match(TokenType::OP_SHR)))
        {
            Token op = previous();
            ExprPtr right = term();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::term()
    {
        ExprPtr expr = factor();
        while (expr && (match(TokenType::OP_ADD) || match(TokenType::OP_SUB)))
        {
            Token op = previous();
            ExprPtr right = factor();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::factor()
    {
        ExprPtr expr = unary();
        while (expr &&
               (match(TokenType::OP_MUL) || match(TokenType::OP_DIV) || match(TokenType::OP_MOD)))
        {
            Token op = previous();
            ExprPtr right = unary();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::unary()
    {
        if (match(TokenType::OP_LNOT) || match(TokenType::KW_NOT) || match(TokenType::OP_BNOT) ||
            match(TokenType::OP_SUB))
        {
            Token op = previous();
            ExprPtr operand = unary();
            if (!operand) return ExprPtr();
            return ExprPtr(new UnaryExpr(op.line, op.column, op.type, std::move(operand)));
        }
        return power();
    }

    ExprPtr Parser::power()
    {
        ExprPtr expr = postfix();
        while (expr && match(TokenType::OP_POW))
        {
            Token op = previous();
            ExprPtr right = power_rhs();
            expr = make_binary(std::move(expr), op, std::move(right));
        }
        return expr;
    }

    ExprPtr Parser::power_rhs()
    {
        if (match(TokenType::OP_LNOT) || match(TokenType::KW_NOT) || match(TokenType::OP_BNOT) ||
            match(TokenType::OP_SUB))
        {
            Token op = previous();
            ExprPtr rhs = power_rhs();
            if (!rhs) return ExprPtr();
            return ExprPtr(new UnaryExpr(op.line, op.column, op.type, std::move(rhs)));
        }
        return postfix();
    }

    ExprPtr Parser::postfix()
    {
        ExprPtr expr = primary();
        if (!expr) return ExprPtr();

        while (true)
        {
            if (match(TokenType::LBRACKET))
            {
                Token open = previous();
                std::vector<ExprPtr> indices = index_list();
                consume(TokenType::RBRACKET, "expected ']' after index list");
                expr.reset(new IndexExpr(open.line, open.column, std::move(expr), std::move(indices)));
                continue;
            }

            if (match(TokenType::LPAREN))
            {
                Token open = previous();
                ArgList args = call_arg_list();
                consume(TokenType::RPAREN, "expected ')' after argument list");
                if (!args.valid) return ExprPtr();
                expr.reset(new CallExpr(open.line, open.column, std::move(expr), std::move(args.args)));
                continue;
            }

            break;
        }

        return expr;
    }

    ExprPtr Parser::primary()
    {
        if (check(TokenType::KW_IF)) return if_expression();

        if (check(TokenType::KW_TRUE) || check(TokenType::KW_FALSE) ||
            check(TokenType::NUMERIC_BASE) ||
            check(TokenType::STRING_LITERAL) || check(TokenType::RAW_STRING_LITERAL))
        {
            return literal();
        }

        if (check(TokenType::IDENTIFIER)) return reference();

        if (match(TokenType::LPAREN))
        {
            Token open = previous();
            ExprPtr inner = expression();
            if (!inner) return ExprPtr();
            consume(TokenType::RPAREN, "expected ')' after grouped expression");
            return ExprPtr(new GroupingExpr(open.line, open.column, std::move(inner)));
        }

        if (check(TokenType::LBRACKET)) return sweep_generator();
        if (check(TokenType::LBRACE)) return matrix_generator();

        report_here("expected expression");
        return ExprPtr();
    }

    ExprPtr Parser::if_expression()
    {
        Token kw = consume(TokenType::KW_IF, "expected 'if'");
        consume(TokenType::LPAREN, "expected '(' after 'if'");
        ExprPtr first_condition = expression();
        consume(TokenType::RPAREN, "expected ')' after if condition");
        consume(TokenType::KW_THEN, "expected 'then' after if condition");
        ExprPtr first_value = expression();

        if (!first_condition || !first_value) return ExprPtr();

        std::vector<IfBranch> branches;
        branches.push_back(IfBranch(std::move(first_condition), std::move(first_value)));

        while (match(TokenType::KW_ELSEIF))
        {
            consume(TokenType::LPAREN, "expected '(' after 'elseif'");
            ExprPtr cond = expression();
            consume(TokenType::RPAREN, "expected ')' after elseif condition");
            consume(TokenType::KW_THEN, "expected 'then' after elseif condition");
            ExprPtr value = expression();
            if (!cond || !value) return ExprPtr();
            branches.push_back(IfBranch(std::move(cond), std::move(value)));
        }

        consume(TokenType::KW_ELSE, "expected 'else' in if-expression");
        ExprPtr else_value = expression();
        if (!else_value) return ExprPtr();

        return ExprPtr(new IfExpr(kw.line, kw.column, std::move(branches), std::move(else_value)));
    }

    ExprPtr Parser::literal()
    {
        if (match(TokenType::KW_TRUE))
        {
            Token t = previous();
            return ExprPtr(new BooleanExpr(t.line, t.column, true));
        }

        if (match(TokenType::KW_FALSE))
        {
            Token t = previous();
            return ExprPtr(new BooleanExpr(t.line, t.column, false));
        }

        if (match(TokenType::NUMERIC_BASE))
        {
            Token base = previous();
            NumberKind kind = classify_number_kind(base.lexeme);
            int radix = classify_radix(base.lexeme, kind);

            std::string suffix;
            if (match(TokenType::NUMERIC_SUFFIX))
                suffix = previous().lexeme;

            return ExprPtr(new NumberExpr(base.line,
                                          base.column,
                                          kind,
                                          base.lexeme,
                                          radix,
                                          suffix));
        }

        if (match(TokenType::STRING_LITERAL))
        {
            Token t = previous();
            std::string decoded;
            if (!decode_string_literal(t.lexeme, &decoded))
            {
                report_at(t, "invalid string literal");
                return ExprPtr();
            }
            return ExprPtr(new StringExpr(t.line, t.column, decoded, false));
        }

        if (match(TokenType::RAW_STRING_LITERAL))
        {
            Token t = previous();
            return ExprPtr(new StringExpr(t.line, t.column, strip_raw_string_quotes(t.lexeme), true));
        }

        report_here("expected literal");
        return ExprPtr();
    }

    ExprPtr Parser::reference()
    {
        Token first = consume(TokenType::IDENTIFIER, "expected identifier");
        std::vector<RefSegment> segments;
        segments.push_back(RefSegment(first.lexeme, RefSeparator::None));

        while (true)
        {
            if (match(TokenType::DOT))
            {
                Token name = consume(TokenType::IDENTIFIER, "expected identifier after '.'");
                segments.push_back(RefSegment(name.lexeme, RefSeparator::Dot));
                continue;
            }
            if (match(TokenType::DDOT))
            {
                Token name = consume(TokenType::IDENTIFIER, "expected identifier after '..'");
                segments.push_back(RefSegment(name.lexeme, RefSeparator::DDot));
                continue;
            }
            break;
        }

        return ExprPtr(new ReferenceExpr(first.line, first.column, std::move(segments)));
    }

    ExprPtr Parser::sweep_generator()
    {
        Token open = consume(TokenType::LBRACKET, "expected '['");
        std::vector<ExprPtr> items = item_list(false);
        consume(TokenType::RBRACKET, "expected ']' after sweep generator");
        return ExprPtr(new SweepExpr(open.line, open.column, std::move(items)));
    }

    ExprPtr Parser::matrix_generator()
    {
        Token open = consume(TokenType::LBRACE, "expected '{'");
        std::vector<ExprPtr> items = item_list(false);
        consume(TokenType::RBRACE, "expected '}' after matrix generator");
        return ExprPtr(new MatrixExpr(open.line, open.column, std::move(items)));
    }

    std::vector<ExprPtr> Parser::item_list(bool allow_null_range)
    {
        std::vector<ExprPtr> items;
        items.push_back(sequence(allow_null_range));
        while (match(TokenType::COMMA))
        {
            items.push_back(sequence(allow_null_range));
        }
        return items;
    }

    std::vector<ExprPtr> Parser::index_list() { return item_list(true); }

    Parser::ArgList Parser::call_arg_list()
    {
        ArgList list;
        list.valid = true;

        if (check(TokenType::RPAREN)) return list;

        while (true)
        {
            if (check(TokenType::COMMA) || check(TokenType::RPAREN))
            {
                list.args.push_back(ExprPtr());
            }
            else
            {
                list.args.push_back(sequence(false));
            }

            if (!match(TokenType::COMMA)) break;
        }

        int last_non_empty = -1;
        for (std::size_t i = 0; i < list.args.size(); ++i)
        {
            if (list.args[i]) last_non_empty = static_cast<int>(i);
        }

        if (last_non_empty < 0)
        {
            report_at(peek(), "pure default argument slots are not allowed; use '()'");
            list.valid = false;
            return list;
        }

        if (last_non_empty != static_cast<int>(list.args.size()) - 1)
        {
            report_at(peek(), "trailing default argument slots are not allowed");
            list.valid = false;
        }

        return list;
    }

    ExprPtr Parser::sequence(bool allow_null_range)
    {
        if (match(TokenType::OP_SEQ))
        {
            Token op = previous();
            if (!allow_null_range)
            {
                report_at(op, "bare '::' is only valid inside index expressions");
                return ExprPtr();
            }
            return ExprPtr(new NullRangeExpr(op.line, op.column));
        }

        ExprPtr start = expression();
        if (!start) return ExprPtr();

        if (!match(TokenType::OP_SEQ)) return start;

        Token op = previous();
        ExprPtr mid = expression();
        if (!mid) return ExprPtr();

        if (match(TokenType::OP_SEQ))
        {
            ExprPtr stop = expression();
            if (!stop) return ExprPtr();
            return ExprPtr(new RangeExpr(op.line,
                                         op.column,
                                         std::move(start),
                                         std::move(mid),
                                         std::move(stop)));
        }

        return ExprPtr(
            new RangeExpr(op.line, op.column, std::move(start), ExprPtr(), std::move(mid)));
    }

    bool Parser::match(TokenType type)
    {
        if (!check(type)) return false;
        advance();
        return true;
    }

    bool Parser::match_any(TokenType a, TokenType b)
    {
        if (match(a)) return true;
        return match(b);
    }

    const Token& Parser::consume(TokenType type, const char* message)
    {
        if (check(type)) return advance();
        report_here(message);
        return peek();
    }

    bool Parser::check(TokenType type) const
    {
        if (is_at_end()) return type == TokenType::END_OF_INPUT;
        return peek().type == type;
    }

    const Token& Parser::advance()
    {
        if (!is_at_end()) ++current_;
        return previous();
    }

    bool Parser::is_at_end() const { return peek().type == TokenType::END_OF_INPUT; }

    const Token& Parser::peek() const { return tokens_[current_]; }

    const Token& Parser::previous() const { return tokens_[current_ - 1]; }

    void Parser::report_here(const char* message) { report_at(peek(), message); }

    void Parser::report_at(const Token& token, const std::string& message)
    {
        Error err;
        err.line = token.line;
        err.column = token.column;
        err.kind = (token.type == TokenType::INVALID)
                       ? ErrorKind::Lexical
                       : ErrorKind::Syntax;

        std::ostringstream oss;
        oss << message;
        oss << "; found " << to_string(token);
        err.message = oss.str();
        errors_.push_back(err);
    }

    ExprPtr Parser::make_binary(ExprPtr left, const Token& op, ExprPtr right)
    {
        if (!left || !right) return ExprPtr();
        return ExprPtr(new BinaryExpr(op.line, op.column, op.type, std::move(left), std::move(right)));
    }

    ExprPtr Parser::make_logical(ExprPtr left, const Token& op, ExprPtr right)
    {
        if (!left || !right) return ExprPtr();
        return ExprPtr(new LogicalExpr(op.line, op.column, op.type, std::move(left), std::move(right)));
    }

    NumberKind Parser::classify_number_kind(const std::string& lexeme)
    {
        if (!lexeme.empty() && lexeme[lexeme.size() - 1] == 'i') return NumberKind::Imaginary;
        if (lexeme.find('.') != std::string::npos || lexeme.find('e') != std::string::npos ||
            lexeme.find('E') != std::string::npos)
        {
            return NumberKind::Real;
        }
        return NumberKind::Integer;
    }

    int Parser::classify_radix(const std::string& lexeme, NumberKind kind)
    {
        if (kind != NumberKind::Integer) return 10;
        if (lexeme.size() >= 3 && lexeme[0] == '0' && (lexeme[1] == 'x' || lexeme[1] == 'X'))
        {
            return 16;
        }
        if (is_octal_lexeme(lexeme)) return 8;
        return 10;
    }

    bool Parser::decode_string_literal(const std::string& lexeme, std::string* out)
    {
        if (!out) return false;
        out->clear();

        if (lexeme.size() < 2 || lexeme[0] != '"' || lexeme[lexeme.size() - 1] != '"') return false;

        for (std::size_t i = 1; i + 1 < lexeme.size(); ++i)
        {
            char c = lexeme[i];
            if (c != '\\')
            {
                out->push_back(c);
                continue;
            }

            if (i + 1 >= lexeme.size() - 1) return false;
            char esc = lexeme[++i];
            switch (esc)
            {
                case 'n': out->push_back('\n'); break;
                case 'r': out->push_back('\r'); break;
                case 'f': out->push_back('\f'); break;
                case 'b': out->push_back('\b'); break;
                case 't': out->push_back('\t'); break;
                case '"': out->push_back('"'); break;
                case '\\': out->push_back('\\'); break;
                case 'x':
                {
                    if (i + 2 >= lexeme.size() - 1) return false;
                    char a = lexeme[i + 1];
                    char b = lexeme[i + 2];
                    if (!std::isxdigit(static_cast<unsigned char>(a)) ||
                        !std::isxdigit(static_cast<unsigned char>(b)))
                    {
                        return false;
                    }

                    const std::string hex = lexeme.substr(i + 1, 2);
                    const int value = std::strtol(hex.c_str(), nullptr, 16);
                    out->push_back(static_cast<char>(value));
                    i += 2;
                    break;
                }
                case '0':
                {
                    if (i + 3 >= lexeme.size() - 1) return false;
                    const char a = lexeme[i + 1];
                    const char b = lexeme[i + 2];
                    const char d = lexeme[i + 3];
                    if (a < '0' || a > '7' || b < '0' || b > '7' || d < '0' || d > '7')
                    {
                        return false;
                    }

                    const std::string oct = lexeme.substr(i + 1, 3);
                    const int value = std::strtol(oct.c_str(), nullptr, 8);
                    out->push_back(static_cast<char>(value));
                    i += 3;
                    break;
                }
                default:
                    return false;
            }
        }

        return true;
    }

    std::string Parser::strip_raw_string_quotes(const std::string& lexeme)
    {
        if (lexeme.size() < 4) return "";
        return lexeme.substr(2, lexeme.size() - 4);
    }
} // namespace rel
