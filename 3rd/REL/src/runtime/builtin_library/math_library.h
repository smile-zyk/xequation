#pragma once

#include "function.h"
#include "value.h"

namespace rel {
namespace math {

// =============================================================================
//  Reduce operation C++ API
// =============================================================================

/// Innermost-dimension minimum.
Value Min(const Value& v);

/// Innermost-dimension maximum.
Value Max(const Value& v);

/// Innermost-dimension sum.
Value Sum(const Value& v);

/// Innermost-dimension mean.
Value Mean(const Value& v);

// =============================================================================
//  Library factory
// =============================================================================

/// Build the "math" function library (trig, log, exp, sqrt, reduce, ...).
FunctionLibrary MakeLibrary();

}  // namespace math
}  // namespace rel
