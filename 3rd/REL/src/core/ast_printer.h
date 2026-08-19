#pragma once

#include "expr.h"

#include <string>

namespace rel
{
    class AstPrinter : public ExprVisitor
    {
    public:
        std::string Print(const Expr& expr);

        void visit_number(const NumberExpr& expr) override;
        void visit_boolean(const BooleanExpr& expr) override;
        void visit_string(const StringExpr& expr) override;
        void visit_reference(const ReferenceExpr& expr) override;
        void visit_unary(const UnaryExpr& expr) override;
        void visit_binary(const BinaryExpr& expr) override;
        void visit_logical(const LogicalExpr& expr) override;
        void visit_conditional(const ConditionalExpr& expr) override;
        void visit_if(const IfExpr& expr) override;
        void visit_call(const CallExpr& expr) override;
        void visit_index(const IndexExpr& expr) override;
        void visit_grouping(const GroupingExpr& expr) override;
        void visit_sweep(const SweepExpr& expr) override;
        void visit_matrix(const MatrixExpr& expr) override;
        void visit_range(const RangeExpr& expr) override;
        void visit_null_range(const NullRangeExpr& expr) override;

    private:
        std::string out_;

        std::string print_ptr(const ExprPtr& expr);
        std::string parenthesize(const std::string& head,
                                 const Expr* first,
                                 const Expr* second = nullptr,
                                 const Expr* third = nullptr);
        std::string token_name(TokenType type) const;
    };
} // namespace rel
