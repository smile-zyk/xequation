#ifndef REL_OPERATION_MATH_OPERATION_H
#define REL_OPERATION_MATH_OPERATION_H

#include "rel_api.h"
#include "value.h"

namespace rel {
namespace operation {

// =========================================================================
//  Math function operation wrappers (public API, called from math_library)
// =========================================================================

// Trigonometric
REL_API Value OperationSin (const Value& v);
REL_API Value OperationCos (const Value& v);
REL_API Value OperationTan (const Value& v);
REL_API Value OperationCot (const Value& v);
// Inverse trigonometric
REL_API Value OperationAsin(const Value& v);
REL_API Value OperationAcos(const Value& v);
REL_API Value OperationAtan(const Value& v);
REL_API Value OperationAcot(const Value& v);
// Hyperbolic
REL_API Value OperationSinh(const Value& v);
REL_API Value OperationCosh(const Value& v);
REL_API Value OperationTanh(const Value& v);
REL_API Value OperationCoth(const Value& v);
// Inverse hyperbolic
REL_API Value OperationAsinh(const Value& v);
REL_API Value OperationAcosh(const Value& v);
REL_API Value OperationAtanh(const Value& v);
REL_API Value OperationAcoth(const Value& v);
// Log / exp
REL_API Value OperationLog  (const Value& v);
REL_API Value OperationLog10(const Value& v);
REL_API Value OperationExp  (const Value& v);
// Power
REL_API Value OperationSqrt(const Value& v);
REL_API Value OperationSqr (const Value& v);
// Rounding
REL_API Value OperationCeil (const Value& v);
REL_API Value OperationFloor(const Value& v);
REL_API Value OperationRound(const Value& v);
REL_API Value OperationFix  (const Value& v);
REL_API Value OperationInt  (const Value& v);
REL_API Value OperationFloat(const Value& v);
// Angle conversion
REL_API Value OperationDeg(const Value& v);
REL_API Value OperationRad(const Value& v);
// Misc
REL_API Value OperationSinc(const Value& v);
REL_API Value OperationStep(const Value& v);
// Absolute / sign
REL_API Value OperationAbs(const Value& v);
REL_API Value OperationSgn(const Value& v);
// Complex
REL_API Value OperationReal (const Value& v);
REL_API Value OperationImag (const Value& v);
REL_API Value OperationConj (const Value& v);
REL_API Value OperationPhase(const Value& v);
// dB
REL_API Value OperationDb    (const Value& v, const Value& z1, const Value& z2);
REL_API Value OperationDbm   (const Value& v, const Value& z);
REL_API Value OperationDbmtow(const Value& v);
REL_API Value OperationWtodbm(const Value& v);

// Binary math
REL_API Value OperationAtan2(const Value& y, const Value& x);
REL_API Value OperationRoot (const Value& x, const Value& n);
REL_API Value OperationMax2 (const Value& a, const Value& b);
REL_API Value OperationMin2 (const Value& a, const Value& b);

}  // namespace operation
}  // namespace rel

#endif  // REL_OPERATION_MATH_OPERATION_H
