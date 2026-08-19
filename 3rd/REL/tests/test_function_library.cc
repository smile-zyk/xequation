// Static function-library extension tests.
//
// Verify the C++ extension model: a static library exposes MakeLibrary(), the
// host registers it explicitly, and its function can call other registered
// functions (sin, cos) through the registry.

#include "rel.h"
#include "environment.h"
#include "function_library_sample.h"

#include <gtest/gtest.h>

namespace
{
    void RegisterAllLibraries()
    {
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::RegisterLibrary(
            rel::function_library_sample::MakeLibrary());
    }
}  // namespace

TEST(FunctionLibrarySampleTest, MakeLibraryRegistersSincos)
{
    rel::Environment env;
    RegisterAllLibraries();

    ASSERT_TRUE(rel::Environment::HasFunction("sincos"));
}

TEST(FunctionLibrarySampleTest, SincosCallsSinThenCos)
{
    rel::Environment env;
    rel::Environment::InitBuiltinConstants();
    RegisterAllLibraries();

    // sincos(PI/4) = sin(PI/4) * cos(PI/4) = 0.5
    rel::Value v = rel::Eval("sincos(PI/4)", &env);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 0.5, 1e-12);
}

TEST(FunctionLibrarySampleTest, UnregisterFunction)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    ASSERT_TRUE(rel::Environment::HasFunction("datasets"));
    EXPECT_TRUE(rel::Environment::UnregisterFunction("datasets"));
    EXPECT_FALSE(rel::Environment::HasFunction("datasets"));

    // Unregistering a nonexistent function returns false.
    EXPECT_FALSE(rel::Environment::UnregisterFunction("datasets"));
    EXPECT_FALSE(rel::Environment::UnregisterFunction("no_such_function"));
}
