#ifndef REL_OPERATION_H
#define REL_OPERATION_H

#include "rel_api.h"
#include "value.h"

#include <vector>

namespace rel {
namespace operation {

// =========================================================================
//  Operation kernels (delegate targets for the operators above)
// =========================================================================

// =========================================================================
// Binary arithmetic
// =========================================================================

REL_API Value OperationAdd(const Value& lhs, const Value& rhs);
REL_API Value OperationSub(const Value& lhs, const Value& rhs);
REL_API Value OperationMul(const Value& lhs, const Value& rhs);
REL_API Value OperationDiv(const Value& lhs, const Value& rhs);
REL_API Value OperationTimes(const Value& lhs, const Value& rhs);
REL_API Value OperationRdivide(const Value& lhs, const Value& rhs);
REL_API Value OperationMod(const Value& lhs, const Value& rhs);
REL_API Value OperationPow(const Value& lhs, const Value& rhs);

// =========================================================================
// Unary
// =========================================================================

REL_API Value OperationNegate(const Value& v);
REL_API Value OperationNot(const Value& v);
REL_API Value OperationBitNot(const Value& v);

// =========================================================================
// Comparison (result is Integer 0/1, dimensionless)
// =========================================================================

REL_API Value OperationEq(const Value& lhs, const Value& rhs);
REL_API Value OperationNeq(const Value& lhs, const Value& rhs);
REL_API Value OperationLt(const Value& lhs, const Value& rhs);
REL_API Value OperationGt(const Value& lhs, const Value& rhs);
REL_API Value OperationLe(const Value& lhs, const Value& rhs);
REL_API Value OperationGe(const Value& lhs, const Value& rhs);

// =========================================================================
// Bitwise (Integer only, dimensionless)
// =========================================================================

REL_API Value OperationBitAnd(const Value& lhs, const Value& rhs);
REL_API Value OperationBitOr(const Value& lhs, const Value& rhs);
REL_API Value OperationBitXor(const Value& lhs, const Value& rhs);

// =========================================================================
// Shift (Integer only)
// =========================================================================

REL_API Value OperationShl(const Value& lhs, const Value& rhs);
REL_API Value OperationShr(const Value& lhs, const Value& rhs);

// =========================================================================
// Logical (result is Integer 0/1, dimensionless)
// =========================================================================

REL_API Value OperationAnd(const Value& lhs, const Value& rhs);
REL_API Value OperationOr(const Value& lhs, const Value& rhs);

// =========================================================================
// Ternary
// =========================================================================

/// Conditional(condition, true_value, false_value) -- ternary operator.
/// condition is evaluated as logical (non-zero -> 1, zero -> 0).
/// For each element, if condition is 1 the result is taken from true_value,
/// otherwise from false_value.  Supports row broadcast and shape broadcast.
REL_API Value OperationConditional(const Value& condition,
                                         const Value& true_value,
                                         const Value& false_value);
/// If(cond0, val0, cond1, val1, ..., cond_{n-1}, val_{n-1}, else_val)
/// -- multi-branch if/elseif/else.  Takes 2n+1 operands (n >= 1).
/// For each element, the first branch whose condition is non-zero provides
/// the result; if no condition matches, the final else_val is used.
/// This generalizes Conditional to an arbitrary number of branches.
REL_API Value OperationIf(const std::vector<Value>& operands);
// =========================================================================
// Variadic generators
// =========================================================================

/// Matrix {} -- stack operands with row broadcast.
REL_API Value OperationMatrix(const std::vector<Value>& operands);

/// Sweep [] -- collect operands into a DataArray (one row per operand).
REL_API Value OperationSweep(const std::vector<Value>& operands);

}  // namespace operation
}  // namespace rel

#endif  // REL_OPERATION_H
