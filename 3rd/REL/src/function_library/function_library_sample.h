#pragma once

#include "function/function_library.h"

namespace rel {
namespace function_library_sample {

// =============================================================================
//  Sample C++ function-library extension
// =============================================================================
//
//  Demonstrates the C++ extension model: a static library that links
//  rel_runtime and exposes a MakeLibrary() factory.  The host (rel.exe or
//  tests) registers the returned FunctionLibrary explicitly via
//  Environment::RegisterLibrary.
//
//  The single function sincos(x) = sin(x) * cos(x) calls the already-registered
//  builtin functions sin() and cos() through the registry — the same path the
//  evaluator uses (Environment::CallFunction).

/// Build the "function_library_sample" library.
FunctionLibrary MakeLibrary();

}  // namespace function_library_sample
}  // namespace rel
