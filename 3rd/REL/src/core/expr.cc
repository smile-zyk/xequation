#include "expr.h"

#include <utility>

namespace rel
{
    RefSegment::RefSegment() : sep(RefSeparator::None) {}

    RefSegment::RefSegment(std::string name_value, RefSeparator sep_value)
        : name(std::move(name_value)), sep(sep_value)
    {
    }

    IfBranch::IfBranch(ExprPtr condition_value, ExprPtr value_value)
        : condition(std::move(condition_value)), value(std::move(value_value))
    {
    }

    Expr::Expr(int line_value, int column_value) : line(line_value), column(column_value) {}

    BooleanExpr::BooleanExpr(int line_value, int column_value, bool value_value)
        : Expr(line_value, column_value), value(value_value)
    {
    }

    void BooleanExpr::accept(ExprVisitor& visitor) const { visitor.visit_boolean(*this); }

    NumberExpr::NumberExpr(int line_value,
                           int column_value,
                           NumberKind kind_value,
                           std::string base_lexeme_value,
                           int radix_value,
                           std::string suffix_value)
        : Expr(line_value, column_value),
          kind(kind_value),
          base_lexeme(std::move(base_lexeme_value)),
          radix(radix_value),
          suffix(std::move(suffix_value))
    {
    }

    void NumberExpr::accept(ExprVisitor& visitor) const { visitor.visit_number(*this); }

    StringExpr::StringExpr(int line_value, int column_value, std::string value_value, bool raw_value)
        : Expr(line_value, column_value), value(std::move(value_value)), raw(raw_value)
    {
    }

    void StringExpr::accept(ExprVisitor& visitor) const { visitor.visit_string(*this); }

    ReferenceExpr::ReferenceExpr(int line_value, int column_value, std::vector<RefSegment> segments_value)
        : Expr(line_value, column_value), segments(std::move(segments_value))
    {
    }

    void ReferenceExpr::accept(ExprVisitor& visitor) const { visitor.visit_reference(*this); }

    UnaryExpr::UnaryExpr(int line_value, int column_value, TokenType op_value, ExprPtr operand_value)
        : Expr(line_value, column_value), op(op_value), operand(std::move(operand_value))
    {
    }

    void UnaryExpr::accept(ExprVisitor& visitor) const { visitor.visit_unary(*this); }

    BinaryExpr::BinaryExpr(int line_value,
                           int column_value,
                           TokenType op_value,
                           ExprPtr left_value,
                           ExprPtr right_value)
        : Expr(line_value, column_value),
          op(op_value),
          left(std::move(left_value)),
          right(std::move(right_value))
    {
    }

    void BinaryExpr::accept(ExprVisitor& visitor) const { visitor.visit_binary(*this); }

    LogicalExpr::LogicalExpr(int line_value,
                             int column_value,
                             TokenType op_value,
                             ExprPtr left_value,
                             ExprPtr right_value)
        : Expr(line_value, column_value),
          op(op_value),
          left(std::move(left_value)),
          right(std::move(right_value))
    {
    }

    void LogicalExpr::accept(ExprVisitor& visitor) const { visitor.visit_logical(*this); }

    ConditionalExpr::ConditionalExpr(int line_value,
                                     int column_value,
                                     ExprPtr condition_value,
                                     ExprPtr then_branch_value,
                                     ExprPtr else_branch_value)
        : Expr(line_value, column_value),
          condition(std::move(condition_value)),
          then_branch(std::move(then_branch_value)),
          else_branch(std::move(else_branch_value))
    {
    }

    void ConditionalExpr::accept(ExprVisitor& visitor) const { visitor.visit_conditional(*this); }

    IfExpr::IfExpr(int line_value,
                   int column_value,
                   std::vector<IfBranch> branches_value,
                   ExprPtr else_value_value)
        : Expr(line_value, column_value),
          branches(std::move(branches_value)),
          else_value(std::move(else_value_value))
    {
    }

    void IfExpr::accept(ExprVisitor& visitor) const { visitor.visit_if(*this); }

    CallExpr::CallExpr(int line_value,
                       int column_value,
                       ExprPtr callee_value,
                       std::vector<ExprPtr> args_value)
        : Expr(line_value, column_value), callee(std::move(callee_value)), args(std::move(args_value))
    {
    }

    void CallExpr::accept(ExprVisitor& visitor) const { visitor.visit_call(*this); }

    IndexExpr::IndexExpr(int line_value,
                         int column_value,
                         ExprPtr object_value,
                         std::vector<ExprPtr> indices_value)
        : Expr(line_value, column_value),
          object(std::move(object_value)),
          indices(std::move(indices_value))
    {
    }

    void IndexExpr::accept(ExprVisitor& visitor) const { visitor.visit_index(*this); }

    GroupingExpr::GroupingExpr(int line_value, int column_value, ExprPtr inner_value)
        : Expr(line_value, column_value), inner(std::move(inner_value))
    {
    }

    void GroupingExpr::accept(ExprVisitor& visitor) const { visitor.visit_grouping(*this); }

    SweepExpr::SweepExpr(int line_value, int column_value, std::vector<ExprPtr> items_value)
        : Expr(line_value, column_value), items(std::move(items_value))
    {
    }

    void SweepExpr::accept(ExprVisitor& visitor) const { visitor.visit_sweep(*this); }

    MatrixExpr::MatrixExpr(int line_value, int column_value, std::vector<ExprPtr> items_value)
        : Expr(line_value, column_value), items(std::move(items_value))
    {
    }

    void MatrixExpr::accept(ExprVisitor& visitor) const { visitor.visit_matrix(*this); }

    RangeExpr::RangeExpr(int line_value,
                         int column_value,
                         ExprPtr start_value,
                         ExprPtr step_value,
                         ExprPtr stop_value)
        : Expr(line_value, column_value),
          start(std::move(start_value)),
          step(std::move(step_value)),
          stop(std::move(stop_value))
    {
    }

    void RangeExpr::accept(ExprVisitor& visitor) const { visitor.visit_range(*this); }

    NullRangeExpr::NullRangeExpr(int line_value, int column_value) : Expr(line_value, column_value) {}

    void NullRangeExpr::accept(ExprVisitor& visitor) const { visitor.visit_null_range(*this); }
} // namespace rel
