#pragma once

#include "rel_api.h"
#include "token.h"

#include <memory>
#include <string>
#include <vector>

namespace rel
{
    class ExprVisitor;

    class Expr;
    class NumberExpr;
    class BooleanExpr;
    class StringExpr;
    class ReferenceExpr;
    class UnaryExpr;
    class BinaryExpr;
    class LogicalExpr;
    class ConditionalExpr;
    class IfExpr;
    class CallExpr;
    class IndexExpr;
    class GroupingExpr;
    class SweepExpr;
    class MatrixExpr;
    class RangeExpr;
    class NullRangeExpr;

    typedef std::unique_ptr<Expr> ExprPtr;

    enum class NumberKind
    {
        Integer,
        Real,
        Imaginary,
    };

    enum class RefSeparator
    {
        None,
        Dot,
        DDot,
    };

    struct REL_API RefSegment
    {
        std::string name;
        RefSeparator sep;

        RefSegment();
        RefSegment(std::string name_value, RefSeparator sep_value);
    };

    struct REL_API IfBranch
    {
        ExprPtr condition;
        ExprPtr value;

        IfBranch(ExprPtr condition_value, ExprPtr value_value);
    };

    class REL_API ExprVisitor
    {
    public:
        virtual ~ExprVisitor() {}

        virtual void visit_number(const NumberExpr& expr) = 0;
        virtual void visit_boolean(const BooleanExpr& expr) = 0;
        virtual void visit_string(const StringExpr& expr) = 0;
        virtual void visit_reference(const ReferenceExpr& expr) = 0;
        virtual void visit_unary(const UnaryExpr& expr) = 0;
        virtual void visit_binary(const BinaryExpr& expr) = 0;
        virtual void visit_logical(const LogicalExpr& expr) = 0;
        virtual void visit_conditional(const ConditionalExpr& expr) = 0;
        virtual void visit_if(const IfExpr& expr) = 0;
        virtual void visit_call(const CallExpr& expr) = 0;
        virtual void visit_index(const IndexExpr& expr) = 0;
        virtual void visit_grouping(const GroupingExpr& expr) = 0;
        virtual void visit_sweep(const SweepExpr& expr) = 0;
        virtual void visit_matrix(const MatrixExpr& expr) = 0;
        virtual void visit_range(const RangeExpr& expr) = 0;
        virtual void visit_null_range(const NullRangeExpr& expr) = 0;
    };

    class REL_API Expr
    {
    public:
        Expr(int line_value, int column_value);
        virtual ~Expr() {}

        // AST nodes are move-only: they own unique_ptr members and are
        // managed exclusively through std::unique_ptr<Expr>.  Explicitly
        // deleting the copy operations keeps MSVC from instantiating the
        // (deleted) implicit copy assignment when exporting the class.
        Expr(const Expr&) = delete;
        Expr& operator=(const Expr&) = delete;
        Expr(Expr&&) noexcept = default;
        Expr& operator=(Expr&&) noexcept = default;

        int line;
        int column;

        virtual void accept(ExprVisitor& visitor) const = 0;
    };

    class REL_API BooleanExpr : public Expr
    {
    public:
        BooleanExpr(int line_value, int column_value, bool value_value);

        bool value;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API NumberExpr : public Expr
    {
    public:
        NumberExpr(int line_value,
                   int column_value,
                   NumberKind kind_value,
                   std::string base_lexeme_value,
                   int radix_value,
                   std::string suffix_value);

        NumberKind kind;
        std::string base_lexeme;
        int radix;
        std::string suffix;  // e.g. "GHz", "cm", "m", "MOhm", or ""

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API StringExpr : public Expr
    {
    public:
        StringExpr(int line_value, int column_value, std::string value_value, bool raw_value);

        std::string value;
        bool raw;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API ReferenceExpr : public Expr
    {
    public:
        ReferenceExpr(int line_value, int column_value, std::vector<RefSegment> segments_value);

        std::vector<RefSegment> segments;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API UnaryExpr : public Expr
    {
    public:
        UnaryExpr(int line_value, int column_value, TokenType op_value, ExprPtr operand_value);

        TokenType op;
        ExprPtr operand;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API BinaryExpr : public Expr
    {
    public:
        BinaryExpr(int line_value,
                   int column_value,
                   TokenType op_value,
                   ExprPtr left_value,
                   ExprPtr right_value);

        TokenType op;
        ExprPtr left;
        ExprPtr right;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API LogicalExpr : public Expr
    {
    public:
        LogicalExpr(int line_value,
                    int column_value,
                    TokenType op_value,
                    ExprPtr left_value,
                    ExprPtr right_value);

        TokenType op;
        ExprPtr left;
        ExprPtr right;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API ConditionalExpr : public Expr
    {
    public:
        ConditionalExpr(int line_value,
                        int column_value,
                        ExprPtr condition_value,
                        ExprPtr then_branch_value,
                        ExprPtr else_branch_value);

        ExprPtr condition;
        ExprPtr then_branch;
        ExprPtr else_branch;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API IfExpr : public Expr
    {
    public:
        IfExpr(int line_value,
               int column_value,
               std::vector<IfBranch> branches_value,
               ExprPtr else_value_value);

        std::vector<IfBranch> branches;
        ExprPtr else_value;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API CallExpr : public Expr
    {
    public:
        CallExpr(int line_value,
                 int column_value,
                 ExprPtr callee_value,
                 std::vector<ExprPtr> args_value);

        ExprPtr callee;
        std::vector<ExprPtr> args;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API IndexExpr : public Expr
    {
    public:
        IndexExpr(int line_value,
                  int column_value,
                  ExprPtr object_value,
                  std::vector<ExprPtr> indices_value);

        ExprPtr object;
        std::vector<ExprPtr> indices;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API GroupingExpr : public Expr
    {
    public:
        GroupingExpr(int line_value, int column_value, ExprPtr inner_value);

        ExprPtr inner;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API SweepExpr : public Expr
    {
    public:
        SweepExpr(int line_value, int column_value, std::vector<ExprPtr> items_value);

        std::vector<ExprPtr> items;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API MatrixExpr : public Expr
    {
    public:
        MatrixExpr(int line_value, int column_value, std::vector<ExprPtr> items_value);

        std::vector<ExprPtr> items;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API RangeExpr : public Expr
    {
    public:
        RangeExpr(int line_value,
                  int column_value,
                  ExprPtr start_value,
                  ExprPtr step_value,
                  ExprPtr stop_value);

        ExprPtr start;
        ExprPtr step;
        ExprPtr stop;

        void accept(ExprVisitor& visitor) const override;
    };

    class REL_API NullRangeExpr : public Expr
    {
    public:
        NullRangeExpr(int line_value, int column_value);

        void accept(ExprVisitor& visitor) const override;
    };
} // namespace rel
