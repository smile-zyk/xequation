#ifndef REL_RUNTIME_OPERATION_MATH_OPERATION_H
#define REL_RUNTIME_OPERATION_MATH_OPERATION_H

#include "rel_runtime_api.h"
#include "value.h"

namespace rel {
namespace operation {

// =========================================================================
//  Math function operation wrappers (public API, called from math_library)
// =========================================================================

// Trigonometric
REL_RUNTIME_API Value OperationSin (const Value& v);
REL_RUNTIME_API Value OperationCos (const Value& v);
REL_RUNTIME_API Value OperationTan (const Value& v);
REL_RUNTIME_API Value OperationCot (const Value& v);
// Inverse trigonometric
REL_RUNTIME_API Value OperationAsin(const Value& v);
REL_RUNTIME_API Value OperationAcos(const Value& v);
REL_RUNTIME_API Value OperationAtan(const Value& v);
REL_RUNTIME_API Value OperationAcot(const Value& v);
// Hyperbolic
REL_RUNTIME_API Value OperationSinh(const Value& v);
REL_RUNTIME_API Value OperationCosh(const Value& v);
REL_RUNTIME_API Value OperationTanh(const Value& v);
REL_RUNTIME_API Value OperationCoth(const Value& v);
// Inverse hyperbolic
REL_RUNTIME_API Value OperationAsinh(const Value& v);
REL_RUNTIME_API Value OperationAcosh(const Value& v);
REL_RUNTIME_API Value OperationAtanh(const Value& v);
REL_RUNTIME_API Value OperationAcoth(const Value& v);
// Log / exp
REL_RUNTIME_API Value OperationLog  (const Value& v);
REL_RUNTIME_API Value OperationLog10(const Value& v);
REL_RUNTIME_API Value OperationExp  (const Value& v);
// Power
REL_RUNTIME_API Value OperationSqrt(const Value& v);
REL_RUNTIME_API Value OperationSqr (const Value& v);
// Rounding
REL_RUNTIME_API Value OperationCeil (const Value& v);
REL_RUNTIME_API Value OperationFloor(const Value& v);
REL_RUNTIME_API Value OperationRound(const Value& v);
REL_RUNTIME_API Value OperationFix  (const Value& v);
REL_RUNTIME_API Value OperationInt  (const Value& v);
REL_RUNTIME_API Value OperationFloat(const Value& v);
// Angle conversion
REL_RUNTIME_API Value OperationDeg(const Value& v);
REL_RUNTIME_API Value OperationRad(const Value& v);
// Misc
REL_RUNTIME_API Value OperationSinc(const Value& v);
REL_RUNTIME_API Value OperationStep(const Value& v);
// Absolute / sign
REL_RUNTIME_API Value OperationAbs(const Value& v);
REL_RUNTIME_API Value OperationSgn(const Value& v);
// Complex
REL_RUNTIME_API Value OperationReal (const Value& v);
REL_RUNTIME_API Value OperationImag (const Value& v);
REL_RUNTIME_API Value OperationConj (const Value& v);
REL_RUNTIME_API Value OperationPhase(const Value& v);
// dB
REL_RUNTIME_API Value OperationDb    (const Value& v, const Value& z1, const Value& z2);
REL_RUNTIME_API Value OperationDbm   (const Value& v, const Value& z);
REL_RUNTIME_API Value OperationDbmtow(const Value& v);
REL_RUNTIME_API Value OperationWtodbm(const Value& v);

// Binary math
REL_RUNTIME_API Value OperationAtan2(const Value& y, const Value& x);
REL_RUNTIME_API Value OperationRoot (const Value& x, const Value& n);
REL_RUNTIME_API Value OperationMax2 (const Value& a, const Value& b);
REL_RUNTIME_API Value OperationMin2 (const Value& a, const Value& b);

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_MATH_OPERATION_H
