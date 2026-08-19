// Math function tests: unit tests for the "math" function library
// (sin, cos, tan, asin, acos, atan, sinh, cosh, tanh, asinh, acosh,
//  atanh, log, ln, log10, exp, sqrt, sqr, abs, sgn,
//  real, re, imag, im, conj, conjg, mag, phase).

#include "rel.h"
#include "environment.h"

#include "data_array.h"
#include "data_series.h"
#include "dataset.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <memory>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // =========================================================================
    //  Trigonometric: sin, cos, tan
    // =========================================================================

    TEST(MathFunctionTest, SinScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        EXPECT_NEAR(rel::Eval("sin(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sin(PI/2)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sin(-PI/2)", &env).as_measurement().as_scalar<double>(), -1.0, 1e-12);
    }

    TEST(MathFunctionTest, CosScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        EXPECT_NEAR(rel::Eval("cos(0)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("cos(PI)", &env).as_measurement().as_scalar<double>(), -1.0, 1e-12);
    }

    TEST(MathFunctionTest, TanScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        EXPECT_NEAR(rel::Eval("tan(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("tan(PI/4)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
    }

    TEST(MathFunctionTest, SinVectorCell)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        // sin({0, PI/2}) -> {0, 1} as a Real vector Measurement.
        rel::Value v = rel::Eval("sin({0, PI/2})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
        EXPECT_EQ(m.data_type(), xdataset::DataType::kReal);
        EXPECT_EQ(m.shape()[0], 2u);
        auto vec = m.as_vector<double>();
        EXPECT_NEAR(vec[0], 0.0, 1e-12);
        EXPECT_NEAR(vec[1], 1.0, 1e-12);
    }

    TEST(MathFunctionTest, CosMatrixCell)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        // cos({{0, PI/2}, {PI, 3*PI/2}}) -> {{1, 0}, {-1, 0}}
        rel::Value v = rel::Eval("cos({{0, PI/2}, {PI, 3*PI/2}})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kMatrix);
        EXPECT_EQ(m.shape()[0], 2u);
        EXPECT_EQ(m.shape()[1], 2u);
        auto mat = m.as_matrix<double>();
        EXPECT_NEAR(mat(0, 0), 1.0, 1e-12);
        EXPECT_NEAR(mat(0, 1), 0.0, 1e-12);
        EXPECT_NEAR(mat(1, 0), -1.0, 1e-12);
        EXPECT_NEAR(mat(1, 1), 0.0, 1e-12);
    }

    // =========================================================================
    //  Inverse trigonometric: asin, acos, atan
    // =========================================================================

    TEST(MathFunctionTest, AsinScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("asin(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("asin(1)", &env).as_measurement().as_scalar<double>(), M_PI / 2.0, 1e-12);
        EXPECT_NEAR(rel::Eval("asin(-1)", &env).as_measurement().as_scalar<double>(), -M_PI / 2.0, 1e-12);
    }

    TEST(MathFunctionTest, AcosScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("acos(1)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("acos(0)", &env).as_measurement().as_scalar<double>(), M_PI / 2.0, 1e-12);
        EXPECT_NEAR(rel::Eval("acos(-1)", &env).as_measurement().as_scalar<double>(), M_PI, 1e-12);
    }

    TEST(MathFunctionTest, AtanScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("atan(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("atan(1)", &env).as_measurement().as_scalar<double>(), M_PI / 4.0, 1e-12);
    }

    // =========================================================================
    //  Hyperbolic: sinh, cosh, tanh
    // =========================================================================

    TEST(MathFunctionTest, SinhScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("sinh(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sinh(1)", &env).as_measurement().as_scalar<double>(), std::sinh(1.0), 1e-12);
    }

    TEST(MathFunctionTest, CoshScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("cosh(0)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("cosh(1)", &env).as_measurement().as_scalar<double>(), std::cosh(1.0), 1e-12);
    }

    TEST(MathFunctionTest, TanhScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("tanh(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("tanh(1)", &env).as_measurement().as_scalar<double>(), std::tanh(1.0), 1e-12);
    }

    // =========================================================================
    //  Inverse hyperbolic: asinh, acosh, atanh
    // =========================================================================

    TEST(MathFunctionTest, AsinhScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("asinh(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("asinh(1)", &env).as_measurement().as_scalar<double>(), std::asinh(1.0), 1e-12);
    }

    TEST(MathFunctionTest, AcoshScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("acosh(1)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("acosh(2)", &env).as_measurement().as_scalar<double>(), std::acosh(2.0), 1e-12);
    }

    TEST(MathFunctionTest, AtanhScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("atanh(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("atanh(0.5)", &env).as_measurement().as_scalar<double>(), std::atanh(0.5), 1e-12);
    }

    // =========================================================================
    //  Logarithms & exponential: log / ln, log10, exp
    // =========================================================================

    TEST(MathFunctionTest, LogScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::InitBuiltinConstants();

        EXPECT_NEAR(rel::Eval("log(1)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        // log(e) ≈ 1.0; the built-in "e" constant is an approximation, so
        // use a tolerance appropriate for the precision of that constant.
        EXPECT_NEAR(rel::Eval("log(e)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-8);
    }

    TEST(MathFunctionTest, LnIsAliasForLog)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        double a = rel::Eval("log(2)", &env).as_measurement().as_scalar<double>();
        double b = rel::Eval("ln(2)", &env).as_measurement().as_scalar<double>();
        EXPECT_DOUBLE_EQ(a, b);
    }

    TEST(MathFunctionTest, Log10Scalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("log10(1)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("log10(10)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("log10(100)", &env).as_measurement().as_scalar<double>(), 2.0, 1e-12);
    }

    TEST(MathFunctionTest, ExpScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("exp(0)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("exp(1)", &env).as_measurement().as_scalar<double>(), std::exp(1.0), 1e-12);
    }

    TEST(MathFunctionTest, LogExpRoundtrip)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("log(exp(3))", &env).as_measurement().as_scalar<double>(), 3.0, 1e-10);
        EXPECT_NEAR(rel::Eval("exp(log(5))", &env).as_measurement().as_scalar<double>(), 5.0, 1e-10);
    }

    // =========================================================================
    //  Power: sqrt, sqr
    // =========================================================================

    TEST(MathFunctionTest, SqrtScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("sqrt(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sqrt(1)", &env).as_measurement().as_scalar<double>(), 1.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sqrt(4)", &env).as_measurement().as_scalar<double>(), 2.0, 1e-12);
        EXPECT_NEAR(rel::Eval("sqrt(2)", &env).as_measurement().as_scalar<double>(), std::sqrt(2.0), 1e-12);
    }

    TEST(MathFunctionTest, SqrScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("sqr(0)", &env).as_measurement().as_scalar<int>(), 0);
        EXPECT_EQ(rel::Eval("sqr(3)", &env).as_measurement().as_scalar<int>(), 9);
        EXPECT_EQ(rel::Eval("sqr(-4)", &env).as_measurement().as_scalar<int>(), 16);
        EXPECT_NEAR(rel::Eval("sqr(0.5)", &env).as_measurement().as_scalar<double>(), 0.25, 1e-12);
    }

    // =========================================================================
    //  Absolute value: abs
    // =========================================================================

    TEST(MathFunctionTest, AbsScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("abs(0)", &env).as_measurement().as_scalar<int>(), 0);
        EXPECT_EQ(rel::Eval("abs(5)", &env).as_measurement().as_scalar<int>(), 5);
        EXPECT_EQ(rel::Eval("abs(-5)", &env).as_measurement().as_scalar<int>(), 5);
    }

    TEST(MathFunctionTest, AbsVectorCell)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("abs({-3, 0, 4})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
        auto vec = m.as_vector<int>();
        EXPECT_EQ(vec[0], 3);
        EXPECT_EQ(vec[1], 0);
        EXPECT_EQ(vec[2], 4);
    }

    // =========================================================================
    //  Sign: sgn
    // =========================================================================

    TEST(MathFunctionTest, SgnScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("sgn(0)", &env).as_measurement().as_scalar<int>(),  0);
        EXPECT_EQ(rel::Eval("sgn(7)", &env).as_measurement().as_scalar<int>(),  1);
        EXPECT_EQ(rel::Eval("sgn(-3)", &env).as_measurement().as_scalar<int>(), -1);
        EXPECT_EQ(rel::Eval("sgn(0.01)", &env).as_measurement().as_scalar<int>(), 1);
        EXPECT_EQ(rel::Eval("sgn(-0.01)", &env).as_measurement().as_scalar<int>(), -1);
    }

    // =========================================================================
    //  Complex: real/re, imag/im
    // =========================================================================

    TEST(MathFunctionTest, RealScalar)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("real(3.5)", &env).as_measurement().as_scalar<double>(), 3.5, 1e-12);
    }

    TEST(MathFunctionTest, ReIsAliasForReal)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        double a = rel::Eval("real(2.5)", &env).as_measurement().as_scalar<double>();
        double b = rel::Eval("re(2.5)", &env).as_measurement().as_scalar<double>();
        EXPECT_DOUBLE_EQ(a, b);
    }

    TEST(MathFunctionTest, ImagRealReturnsZero)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("imag(5)", &env).as_measurement().as_scalar<int>(), 0);
        EXPECT_NEAR(rel::Eval("imag(-2.5)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
    }

    TEST(MathFunctionTest, ImIsAliasForImag)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        int a = rel::Eval("imag(3)", &env).as_measurement().as_scalar<int>();
        int b = rel::Eval("im(3)", &env).as_measurement().as_scalar<int>();
        EXPECT_EQ(a, b);
    }

    // =========================================================================
    //  Complex: conj/conjg
    // =========================================================================

    TEST(MathFunctionTest, ConjRealIsIdentity)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        // conj on a real number is identity (returns same Measurement type).
        rel::Value v = rel::Eval("conj(5.0)", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 5.0, 1e-12);

        v = rel::Eval("conj(-3.14)", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_NEAR(v.as_measurement().as_scalar<double>(), -3.14, 1e-12);
    }

    TEST(MathFunctionTest, ConjgIsAliasForConj)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        double a = rel::Eval("conj(7.0)", &env).as_measurement().as_scalar<double>();
        double b = rel::Eval("conjg(7.0)", &env).as_measurement().as_scalar<double>();
        EXPECT_DOUBLE_EQ(a, b);
    }

    // =========================================================================
    //  Complex: mag, phase
    // =========================================================================

    TEST(MathFunctionTest, MagEqualsAbs)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        int a = rel::Eval("mag(4)", &env).as_measurement().as_scalar<int>();
        int b = rel::Eval("abs(4)", &env).as_measurement().as_scalar<int>();
        EXPECT_EQ(a, b);

        int c = rel::Eval("mag(-3)", &env).as_measurement().as_scalar<int>();
        int d = rel::Eval("abs(-3)", &env).as_measurement().as_scalar<int>();
        EXPECT_EQ(c, d);
    }

    TEST(MathFunctionTest, PhaseReal)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        // phase returns degrees (see ExecutePhase: std::arg(x) * 180/pi).
        EXPECT_NEAR(rel::Eval("phase(1)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("phase(0)", &env).as_measurement().as_scalar<double>(), 0.0, 1e-12);
        EXPECT_NEAR(rel::Eval("phase(-1)", &env).as_measurement().as_scalar<double>(), 180.0, 1e-12);
    }

    // =========================================================================
    //  Integer promotion to Real
    // =========================================================================

    TEST(MathFunctionTest, IntegerPromotesToReal)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("sin(1)", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kReal);

        v = rel::Eval("exp(2)", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kReal);

        v = rel::Eval("sqrt(9)", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kReal);
    }

    TEST(MathFunctionTest, IntegerVectorPromotesToReal)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("sin({0, 1, 2})", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kReal);

        v = rel::Eval("cos({0, 1, 2})", &env);
        ASSERT_TRUE(v.is_measurement());
        EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kReal);
    }

    // =========================================================================
    //  DataArray (row-by-row transform)
    // =========================================================================

    TEST(MathFunctionTest, DataArrayRowByRow)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("sin([0, 1, 2])", &env);
        ASSERT_TRUE(v.is_data_array());
        const xdataset::DataArray& da = v.as_data_array();
        EXPECT_EQ(da.data().size(), 3u);
        EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
        EXPECT_NEAR(da.data().scalar_at<double>(0), 0.0, 1e-12);
        EXPECT_NEAR(da.data().scalar_at<double>(1), std::sin(1.0), 1e-12);
        EXPECT_NEAR(da.data().scalar_at<double>(2), std::sin(2.0), 1e-12);
    }

    TEST(MathFunctionTest, LogOnDataArray)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("log([1, 10, 100])", &env);
        ASSERT_TRUE(v.is_data_array());
        const xdataset::DataArray& da = v.as_data_array();
        EXPECT_EQ(da.data().size(), 3u);
        EXPECT_NEAR(da.data().scalar_at<double>(0), 0.0, 1e-12);
        EXPECT_NEAR(da.data().scalar_at<double>(1), std::log(10.0), 1e-12);
        EXPECT_NEAR(da.data().scalar_at<double>(2), std::log(100.0), 1e-12);
    }

    // =========================================================================
    //  String rejection
    // =========================================================================

    TEST(MathFunctionTest, SinRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("sin(\"abc\")", &env), std::runtime_error);
    }

    TEST(MathFunctionTest, CosRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("cos(\"abc\")", &env), std::runtime_error);
    }

    TEST(MathFunctionTest, LogRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("log(\"x\")", &env), std::runtime_error);
    }

    TEST(MathFunctionTest, SqrtRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("sqrt(\"abc\")", &env), std::runtime_error);
    }

    TEST(MathFunctionTest, ExpRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("exp(\"x\")", &env), std::runtime_error);
    }

    TEST(MathFunctionTest, AbsRejectsString)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();
        EXPECT_THROW(rel::Eval("abs(\"x\")", &env), std::runtime_error);
    }

    // =========================================================================
    //  Boolean treated as Integer (true -> 1, false -> 0)
    // =========================================================================

    TEST(MathFunctionTest, SinBoolean)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("sin(TRUE)", &env).as_measurement().as_scalar<double>(),
                    std::sin(1.0), 1e-12);
        EXPECT_NEAR(rel::Eval("sin(FALSE)", &env).as_measurement().as_scalar<double>(),
                    std::sin(0.0), 1e-12);
    }

    TEST(MathFunctionTest, AbsBoolean)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("abs(TRUE)", &env).as_measurement().as_scalar<int>(), 1);
        EXPECT_EQ(rel::Eval("abs(FALSE)", &env).as_measurement().as_scalar<int>(), 0);
    }

    // =========================================================================
    //  Vector & matrix cell measurements (via inline syntax)
    // =========================================================================

    TEST(MathFunctionTest, SqrtVector)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("sqrt({0, 1, 4, 9})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
        EXPECT_EQ(m.shape()[0], 4u);
        auto vec = m.as_vector<double>();
        EXPECT_NEAR(vec[0], 0.0, 1e-12);
        EXPECT_NEAR(vec[1], 1.0, 1e-12);
        EXPECT_NEAR(vec[2], 2.0, 1e-12);
        EXPECT_NEAR(vec[3], 3.0, 1e-12);
    }

    TEST(MathFunctionTest, ExpVector)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("exp({0, 1})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
        auto vec = m.as_vector<double>();
        EXPECT_NEAR(vec[0], 1.0, 1e-12);
        EXPECT_NEAR(vec[1], std::exp(1.0), 1e-12);
    }

    TEST(MathFunctionTest, Log10Vector)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("log10({1, 10, 100})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
        auto vec = m.as_vector<double>();
        EXPECT_NEAR(vec[0], 0.0, 1e-12);
        EXPECT_NEAR(vec[1], 1.0, 1e-12);
        EXPECT_NEAR(vec[2], 2.0, 1e-12);
    }

    TEST(MathFunctionTest, AsinAcosAtanVector)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        {
            rel::Value v = rel::Eval("asin({0, 1})", &env);
            ASSERT_TRUE(v.is_measurement());
            auto vec = v.as_measurement().as_vector<double>();
            EXPECT_NEAR(vec[0], 0.0, 1e-12);
            EXPECT_NEAR(vec[1], M_PI / 2.0, 1e-12);
        }
        {
            rel::Value v = rel::Eval("acos({1, 0})", &env);
            ASSERT_TRUE(v.is_measurement());
            auto vec = v.as_measurement().as_vector<double>();
            EXPECT_NEAR(vec[0], 0.0, 1e-12);
            EXPECT_NEAR(vec[1], M_PI / 2.0, 1e-12);
        }
        {
            rel::Value v = rel::Eval("atan({0, 1})", &env);
            ASSERT_TRUE(v.is_measurement());
            auto vec = v.as_measurement().as_vector<double>();
            EXPECT_NEAR(vec[0], 0.0, 1e-12);
            EXPECT_NEAR(vec[1], M_PI / 4.0, 1e-12);
        }
    }

    // =========================================================================
    //  Matrix cell measurements
    // =========================================================================

    TEST(MathFunctionTest, SqrMatrix)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("sqr({{0, 1}, {2, 3}})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kMatrix);
        auto mat = m.as_matrix<int>();
        EXPECT_EQ(mat(0, 0), 0);
        EXPECT_EQ(mat(0, 1), 1);
        EXPECT_EQ(mat(1, 0), 4);
        EXPECT_EQ(mat(1, 1), 9);
    }

    TEST(MathFunctionTest, AbsMatrix)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("abs({{-1, 2}, {-3, 4}})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kMatrix);
        auto mat = m.as_matrix<int>();
        EXPECT_EQ(mat(0, 0), 1);
        EXPECT_EQ(mat(0, 1), 2);
        EXPECT_EQ(mat(1, 0), 3);
        EXPECT_EQ(mat(1, 1), 4);
    }

    TEST(MathFunctionTest, ExpMatrix)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        rel::Value v = rel::Eval("exp({{0, 1}, {2, 3}})", &env);
        ASSERT_TRUE(v.is_measurement());
        const xdataset::Measurement& m = v.as_measurement();
        EXPECT_EQ(m.data_kind(), xdataset::DataKind::kMatrix);
        auto mat = m.as_matrix<double>();
        EXPECT_NEAR(mat(0, 0), 1.0, 1e-12);
        EXPECT_NEAR(mat(0, 1), std::exp(1.0), 1e-12);
        EXPECT_NEAR(mat(1, 0), std::exp(2.0), 1e-12);
        EXPECT_NEAR(mat(1, 1), std::exp(3.0), 1e-12);
    }

    // =========================================================================
    //  Compositions
    // =========================================================================

    TEST(MathFunctionTest, ComposeSinCos)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("sin(cos(0))", &env).as_measurement().as_scalar<double>(),
                    std::sin(std::cos(0.0)), 1e-12);
    }

    TEST(MathFunctionTest, ComposeLogExp)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("log(exp(5))", &env).as_measurement().as_scalar<double>(), 5.0, 1e-10);
    }

    TEST(MathFunctionTest, ComposeSqrtSqr)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_NEAR(rel::Eval("sqrt(sqr(7))", &env).as_measurement().as_scalar<double>(), 7.0, 1e-10);
    }

    TEST(MathFunctionTest, ComposeAbsAndSgn)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinFunctions();

        EXPECT_EQ(rel::Eval("sgn(abs(-5))", &env).as_measurement().as_scalar<int>(), 1);
        EXPECT_EQ(rel::Eval("sgn(abs(3))", &env).as_measurement().as_scalar<int>(), 1);
    }

} // namespace
