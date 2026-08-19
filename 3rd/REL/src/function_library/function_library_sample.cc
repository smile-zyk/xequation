// Sample static C++ function-library extension.
//
// A C++ extension is a static library that links rel_runtime (for rel::Value
// and the registry services), builds a FunctionLibrary, and exposes a
// MakeLibrary() factory.  The host registers it explicitly.
//
// sincos(x) = sin(x) * cos(x) — calls sin first, then cos, through the registry.

#include "function_library_sample.h"

#include "environment.h"

#include <string>
#include <vector>

namespace rel {
namespace function_library_sample {

FunctionLibrary MakeLibrary()
{
    FunctionLibrary lib("function_library_sample");

    lib.Add(Function("sincos",
        std::vector<FunctionParam>{ Param("x") },
        [](const Function::ArgMap& args) -> Value {
            const Value& x = args.at("x");

            // Call sin first, then cos, through the registry (the same path
            // the evaluator uses).  Positional (variadic) form.
            Value s = Environment::CallFunction("sin", x);
            Value c = Environment::CallFunction("cos", x);
            
            return Environment::CallFunction("times", s, c);
        }));

    return lib;
}

}  // namespace function_library_sample
}  // namespace rel
