#include "ast_printer.h"

#include <sstream>

namespace rel
{
    namespace
    {
        std::string quote(const std::string& value)
        {
            std::ostringstream oss;
            oss << '"';
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const char c = value[i];
                switch (c)
                {
                    case '\\': oss << "\\\\"; break;
                    case '"': oss << "\\\""; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    case '\f': oss << "\\f"; break;
                    case '\b': oss << "\\b"; break;
                    default: oss << c; break;
                }
            }
            oss << '"';
            return oss.str();
        }
    }

    std::string AstPrinter::Print(const Expr& expr)
    {
        expr.accept(*this);
        return out_;
    }

    std::string AstPrinter::print_ptr(const ExprPtr& expr)
    {
        if (!expr) return "<empty>";
        return Print(*expr);
    }

    std::string AstPrinter::parenthesize(const std::string& head,
                                         const Expr* first,
                                         const Expr* second,
                                         const Expr* third)
    {
        std::ostringstream oss;
        oss << '(' << head;
        if (first)
        {
            oss << ' ' << Print(*first);
        }
        if (second)
        {
            oss << ' ' << Print(*second);
        }
        if (third)
        {
            oss << ' ' << Print(*third);
        }
        oss << ')';
        return oss.str();
    }

    std::string AstPrinter::token_name(TokenType type) const
    {
        switch (type)
        {
            case TokenType::KW_AND:
            case TokenType::OP_LAND:
                return "and";
            case TokenType::KW_OR:
            case TokenType::OP_LOR:
                return "or";
            case TokenType::KW_NOT:
            case TokenType::OP_LNOT:
                return "not";
            case TokenType::KW_EQUALS:
            case TokenType::OP_EQ:
                return "==";
            case TokenType::KW_NOTEQUALS:
            case TokenType::OP_NE:
                return "!=";
            case TokenType::OP_POW:
                return "**";
            case TokenType::OP_SHL:
                return "<<";
            case TokenType::OP_SHR:
                return ">>";
            case TokenType::OP_GE:
                return ">=";
            case TokenType::OP_LE:
                return "<=";
            case TokenType::OP_LT:
                return "<";
            case TokenType::OP_GT:
                return ">";
            case TokenType::OP_ADD:
                return "+";
            case TokenType::OP_SUB:
                return "-";
            case TokenType::OP_MUL:
                return "*";
            case TokenType::OP_DIV:
                return "/";
            case TokenType::OP_MOD:
                return "%";
            case TokenType::OP_BXOR:
                return "^";
            case TokenType::OP_BOR:
                return "|";
            case TokenType::OP_BAND:
                return "&";
            case TokenType::OP_BNOT:
                return "~";
            default:
                return to_string(type);
        }
    }

    void AstPrinter::visit_boolean(const BooleanExpr& expr)
    {
        out_ = expr.value ? "TRUE" : "FALSE";
    }

    void AstPrinter::visit_number(const NumberExpr& expr)
    {
        out_ = expr.base_lexeme + expr.suffix;
    }

    void AstPrinter::visit_string(const StringExpr& expr)
    {
        if (expr.raw)
        {
            out_ = "raw(" + quote(expr.value) + ")";
            return;
        }
        out_ = quote(expr.value);
    }

    void AstPrinter::visit_reference(const ReferenceExpr& expr)
    {
        std::ostringstream oss;
        for (std::size_t i = 0; i < expr.segments.size(); ++i)
        {
            const RefSegment& seg = expr.segments[i];
            if (i > 0)
            {
                if (seg.sep == RefSeparator::Dot) oss << '.';
                if (seg.sep == RefSeparator::DDot) oss << "..";
            }
            oss << seg.name;
        }
        out_ = oss.str();
    }

    void AstPrinter::visit_unary(const UnaryExpr& expr)
    {
        out_ = parenthesize(token_name(expr.op), expr.operand.get());
    }

    void AstPrinter::visit_binary(const BinaryExpr& expr)
    {
        out_ = parenthesize(token_name(expr.op), expr.left.get(), expr.right.get());
    }

    void AstPrinter::visit_logical(const LogicalExpr& expr)
    {
        out_ = parenthesize(token_name(expr.op), expr.left.get(), expr.right.get());
    }

    void AstPrinter::visit_conditional(const ConditionalExpr& expr)
    {
        out_ = parenthesize("?:", expr.condition.get(), expr.then_branch.get(), expr.else_branch.get());
    }

    void AstPrinter::visit_if(const IfExpr& expr)
    {
        std::ostringstream oss;
        oss << "(if";
        for (std::size_t i = 0; i < expr.branches.size(); ++i)
        {
            oss << " (branch " << Print(*expr.branches[i].condition) << ' '
                << Print(*expr.branches[i].value) << ')';
        }
        oss << " (else " << Print(*expr.else_value) << ')' << ')';
        out_ = oss.str();
    }

    void AstPrinter::visit_call(const CallExpr& expr)
    {
        std::ostringstream oss;
        oss << "(call " << Print(*expr.callee);
        for (std::size_t i = 0; i < expr.args.size(); ++i)
        {
            oss << ' ' << print_ptr(expr.args[i]);
        }
        oss << ')';
        out_ = oss.str();
    }

    void AstPrinter::visit_index(const IndexExpr& expr)
    {
        std::ostringstream oss;
        oss << "(index " << Print(*expr.object);
        for (std::size_t i = 0; i < expr.indices.size(); ++i)
        {
            oss << ' ' << Print(*expr.indices[i]);
        }
        oss << ')';
        out_ = oss.str();
    }

    void AstPrinter::visit_grouping(const GroupingExpr& expr)
    {
        out_ = parenthesize("group", expr.inner.get());
    }

    void AstPrinter::visit_sweep(const SweepExpr& expr)
    {
        std::ostringstream oss;
        oss << "[";
        for (std::size_t i = 0; i < expr.items.size(); ++i)
        {
            if (i > 0) oss << ", ";
            oss << Print(*expr.items[i]);
        }
        oss << "]";
        out_ = oss.str();
    }

    void AstPrinter::visit_matrix(const MatrixExpr& expr)
    {
        std::ostringstream oss;
        oss << "{";
        for (std::size_t i = 0; i < expr.items.size(); ++i)
        {
            if (i > 0) oss << ", ";
            oss << Print(*expr.items[i]);
        }
        oss << "}";
        out_ = oss.str();
    }

    void AstPrinter::visit_range(const RangeExpr& expr)
    {
        std::ostringstream oss;
        if (expr.step)
        {
            oss << "(:: " << Print(*expr.start) << ' ' << Print(*expr.step) << ' '
                << Print(*expr.stop) << ')';
        }
        else
        {
            oss << "(:: " << Print(*expr.start) << ' ' << Print(*expr.stop) << ')';
        }
        out_ = oss.str();
    }

    void AstPrinter::visit_null_range(const NullRangeExpr&) { out_ = "::"; }
} // namespace rel
