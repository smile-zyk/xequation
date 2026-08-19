// Builtin function tests: runtime introspection (datasets, default_dataset,
// variables) and the math library (sin, cos, tan, log, ln, log10).

#include "rel.h"
#include "environment.h"

#include "data_array.h"
#include "data_series.h"
#include "dataset.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    xdataset::BlockCreateInfo make_block_info()
    {
        xdataset::BlockCreateInfo info;
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "freq",
                xdataset::DataSeries::CreateScalar<double>(2),
                xdataset::DimensionSpec::Regular(2)});
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "Vout",
                xdataset::DataSeries::CreateScalar<double>(2)});
        return info;
    }

    /// Block with dotted dependent names for fallback-resolution tests.
    xdataset::BlockCreateInfo make_dotted_block_info()
    {
        xdataset::BlockCreateInfo info;
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "time",
                xdataset::DataSeries::CreateScalar<double>(3),
                xdataset::DimensionSpec::Regular(3)});
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "SRC1.i",
                xdataset::DataSeries::CreateScalar<double>(3)});
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "SRC1.v",
                xdataset::DataSeries::CreateScalar<double>(3)});
        return info;
    }

    /// Read the string rows out of a print builtin's result.
    std::vector<std::string> payload(const rel::Value& v)
    {
        EXPECT_TRUE(v.is_data_array());
        const xdataset::DataArray& da = v.as_data_array();
        EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
        EXPECT_EQ(da.data().data_type(), xdataset::DataType::kString);

        std::vector<std::string> rows;
        rows.reserve(static_cast<std::size_t>(da.data().size()));
        for (std::size_t i = 0; i < static_cast<std::size_t>(da.data().size()); ++i)
            rows.push_back(da.data().scalar_at<std::string>(static_cast<xdataset::Index>(i)));
        return rows;
    }
} // namespace

// =========================================================================
//  datasets
// =========================================================================

TEST(BuiltinFunctionTest, DatasetsEmpty)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    rel::Value v = rel::Eval("datasets()", &env);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().data_type(), xdataset::DataType::kString);

    std::vector<std::string> rows = payload(v);
    EXPECT_TRUE(rows.empty());  // no datasets -> no rows
}

TEST(BuiltinFunctionTest, DatasetsWithEntries)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));

    std::vector<std::string> rows = payload(rel::Eval("datasets()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "noise");
}

// =========================================================================
//  default_dataset
// =========================================================================

TEST(BuiltinFunctionTest, DefaultDatasetNone)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("default_dataset()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "NO DEFAULT DATASET");
}

TEST(BuiltinFunctionTest, DefaultDatasetDefault)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    std::vector<std::string> rows = payload(rel::Eval("default_dataset()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "noise");
}

// =========================================================================
//  variables
// =========================================================================

TEST(BuiltinFunctionTest, Variables)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // variables() returns an empty list: user variables are per-Environment
    // and not accessible from global builtins.
    std::vector<std::string> rows = payload(rel::Eval("variables()", &env));
    EXPECT_TRUE(rows.empty());
}

// =========================================================================
//  what
// =========================================================================

TEST(BuiltinFunctionTest, WhatScalarInteger)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what(3)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[2], "Dimension: [1]");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatScalarReal)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what(3.5)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Real");
}

TEST(BuiltinFunctionTest, WhatString)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what(\"abc\")", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[4], "Data Type: String");
}

TEST(BuiltinFunctionTest, WhatBoolean)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what(1 == 1)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[4], "Data Type: Boolean");
}

TEST(BuiltinFunctionTest, WhatVector)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what({1, 2, 3})", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[3], "Data Shape: Vector(3)");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatMatrix)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what({{1, 2}, {3, 4}})", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[3], "Data Shape: Matrix(2, 2)");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatSweepArray)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::vector<std::string> rows = payload(rel::Eval("what([1, 2, 3])", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[2], "Dimension: [3]");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatDatasetVariable)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());  // freq(2) indep, Vout dep
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // Vout: dependent on freq; the parser resolves it via unique lookup.
    std::vector<std::string> rows = payload(rel::Eval("what(Vout)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: [freq]");
    EXPECT_EQ(rows[1], "Kind: Dependent");
    EXPECT_EQ(rows[2], "Dimension: [2]");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Real");
}

// =========================================================================
//  indep
// =========================================================================

TEST(BuiltinFunctionTest, IndepByIndex)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());  // freq indep (2), Vout dep
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // indep(Vout, 1) — extract the first independent variable by 1-based index.
    rel::Value v = rel::Eval("indep(Vout, 1)", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(da.data().size(), 2u);
    EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
}

TEST(BuiltinFunctionTest, IndepByName)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // indep(Vout, "freq") — extract by independent variable name.
    rel::Value v = rel::Eval("indep(Vout, \"freq\")", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(BuiltinFunctionTest, IndepDefaultSelector)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // indep(Vout) — selector defaults to 1 (first independent variable).
    rel::Value v = rel::Eval("indep(Vout)", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(BuiltinFunctionTest, IndepRequiresDataArray)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // First argument must be a DataArray.
    EXPECT_THROW(rel::Eval("indep(1, 1)", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, IndepRequiresIntOrString)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // Second argument must be Integer or String — a Real is rejected.
    EXPECT_THROW(rel::Eval("indep(Vout, 1.5)", &env), std::runtime_error);
}

// =========================================================================
//  min / max
// =========================================================================

TEST(BuiltinFunctionTest, MinOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // [3, 1, 2] -> reduce innermost dimension -> min = 1
    rel::Value v = rel::Eval("min([3, 1, 2])", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().scalar_at<int>(0), 1);
}

TEST(BuiltinFunctionTest, MaxOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // [3, 1, 2] -> max = 3
    rel::Value v = rel::Eval("max([3, 1, 2])", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().scalar_at<int>(0), 3);
}

TEST(BuiltinFunctionTest, MinMaxOfRealSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    rel::Value mn = rel::Eval("min([2.5, 0.5, 1.5])", &env);
    EXPECT_DOUBLE_EQ(mn.as_data_array().data().scalar_at<double>(0), 0.5);

    rel::Value mx = rel::Eval("max([2.5, 0.5, 1.5])", &env);
    EXPECT_DOUBLE_EQ(mx.as_data_array().data().scalar_at<double>(0), 2.5);
}

TEST(BuiltinFunctionTest, MinMaxOnMeasurement)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // min/max now work on scalar Measurement-backed Values (via unified Value API).
    rel::Value mn1 = rel::Eval("min(1)", &env);
    ASSERT_TRUE(mn1.is_data_array());
    EXPECT_EQ(mn1.as_data_array().data().scalar_at<int>(0), 1);

    rel::Value mx1 = rel::Eval("max(1)", &env);
    ASSERT_TRUE(mx1.is_data_array());
    EXPECT_EQ(mx1.as_data_array().data().scalar_at<int>(0), 1);

    rel::Value mn2 = rel::Eval("min(3.5)", &env);
    ASSERT_TRUE(mn2.is_data_array());
    EXPECT_DOUBLE_EQ(mn2.as_data_array().data().scalar_at<double>(0), 3.5);

    rel::Value mx2 = rel::Eval("max(3.5)", &env);
    ASSERT_TRUE(mx2.is_data_array());
    EXPECT_DOUBLE_EQ(mx2.as_data_array().data().scalar_at<double>(0), 3.5);
}

TEST(BuiltinFunctionTest, MinMaxOfDatasetVariable)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // Vout is a Dependent DataArray with 2 rows (freq sweep).
    rel::Value mn = rel::Eval("min(Vout)", &env);
    ASSERT_TRUE(mn.is_data_array());
    EXPECT_EQ(mn.as_data_array().data().size(), 1u);

    rel::Value mx = rel::Eval("max(Vout)", &env);
    ASSERT_TRUE(mx.is_data_array());
    EXPECT_EQ(mx.as_data_array().data().size(), 1u);
}

// =========================================================================
//  output
// =========================================================================

TEST(BuiltinFunctionTest, OutputWritesCsv)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // Default variable_name "data" -> writes "data.csv" in the current dir,
    // returns the absolute path.
    rel::Value ret = rel::Eval("output([1, 2, 3])", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("data.csv"), std::string::npos);
    EXPECT_TRUE(path.size() > 8 && (path[0] == '/' || path[1] == ':'));  // absolute

    // Verify the file exists at the returned path and has expected content.
    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    f.close();

    // Column header named "data"; data rows present.
    EXPECT_NE(content.find("data"), std::string::npos);
    EXPECT_NE(content.find("1"), std::string::npos);
    EXPECT_NE(content.find("3"), std::string::npos);

    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputCustomName)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // Custom variable_name -> writes "<name>.csv", header uses the name.
    rel::Value ret = rel::Eval("output([1, 2, 3], \"result\")", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("result.csv"), std::string::npos);

    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();
    EXPECT_NE(ss.str().find("result"), std::string::npos);

    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputDatasetVariable)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    rel::Value ret = rel::Eval("output(Vout, \"vout\")", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("vout.csv"), std::string::npos);

    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    EXPECT_NE(ss.str().find("freq"), std::string::npos);  // independent header
    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputRequiresDataArray)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // First argument must be a DataArray.
    EXPECT_THROW(rel::Eval("output(1)", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, OutputRequiresStringName)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // Second argument must be a String variable name.
    EXPECT_THROW(rel::Eval("output([1], 1)", &env), std::runtime_error);
}

// =========================================================================
//  math library: sin / cos / tan / log / ln / log10
// =========================================================================

TEST(BuiltinFunctionTest, MathSinOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // sin([0, PI/2]) -> [0, 1] as a Real DataArray.
    rel::Value v = rel::Eval("sin([0, PI/2])", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
    EXPECT_EQ(da.data().size(), 2u);
    EXPECT_NEAR(da.data().scalar_at<double>(0), 0.0, 1e-12);
    EXPECT_NEAR(da.data().scalar_at<double>(1), 1.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathCosOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // cos([0, PI]) -> [1, -1].
    rel::Value v = rel::Eval("cos([0, PI])", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_NEAR(da.data().scalar_at<double>(0), 1.0, 1e-12);
    EXPECT_NEAR(da.data().scalar_at<double>(1), -1.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathTanOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // tan([0, PI/4]) -> [0, 1].
    rel::Value v = rel::Eval("tan([0, PI/4])", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_NEAR(da.data().scalar_at<double>(0), 0.0, 1e-12);
    EXPECT_NEAR(da.data().scalar_at<double>(1), 1.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathLogAndLn)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // log(e) == ln(e) == 1 (natural log), log10(1000) == 3.
    // (e is an approximation of the true constant, so use a loose tolerance.)
    rel::Value l = rel::Eval("log(e)", &env);
    ASSERT_TRUE(l.is_measurement());
    EXPECT_NEAR(l.as_measurement().as_scalar<double>(), 1.0, 1e-6);

    rel::Value ln = rel::Eval("ln(e)", &env);
    ASSERT_TRUE(ln.is_measurement());
    EXPECT_NEAR(ln.as_measurement().as_scalar<double>(), 1.0, 1e-6);

    rel::Value lg = rel::Eval("log10(1000)", &env);
    ASSERT_TRUE(lg.is_measurement());
    EXPECT_NEAR(lg.as_measurement().as_scalar<double>(), 3.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathComplexInput)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // sin(1i) = i * sinh(1) — a Complex result from a Complex input.
    rel::Value v = rel::Eval("sin(1i)", &env);
    ASSERT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_type(), xdataset::DataType::kComplex);
    EXPECT_NEAR(m.as_scalar<std::complex<double>>().real(), 0.0, 1e-12);
    EXPECT_NEAR(m.as_scalar<std::complex<double>>().imag(), std::sinh(1.0), 1e-12);
}

TEST(BuiltinFunctionTest, MathComplexVectorCell)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // sin({0i, 1i}) -> {0, i*sinh(1)} as a Complex vector.
    rel::Value v = rel::Eval("sin({0i, 1i})", &env);
    ASSERT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kComplex);
    EXPECT_NEAR(m.as_vector<std::complex<double>>()[0].imag(), 0.0, 1e-12);
    EXPECT_NEAR(m.as_vector<std::complex<double>>()[1].real(), 0.0, 1e-12);
    EXPECT_NEAR(m.as_vector<std::complex<double>>()[1].imag(), std::sinh(1.0), 1e-12);
}

TEST(BuiltinFunctionTest, MathLogOfSweep)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // log10([1, 100]) -> [0, 2].
    rel::Value v = rel::Eval("log10([1, 100])", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
    EXPECT_NEAR(da.data().scalar_at<double>(0), 0.0, 1e-12);
    EXPECT_NEAR(da.data().scalar_at<double>(1), 2.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathIntegerInputPromotesToReal)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // Integer array [0, 1, 2] -> sin -> Real.
    rel::Value v = rel::Eval("sin([0, 1, 2])", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().data_type(), xdataset::DataType::kReal);
}

TEST(BuiltinFunctionTest, MathScalarInput)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    rel::Value v = rel::Eval("sin(0)", &env);
    ASSERT_TRUE(v.is_measurement());
    EXPECT_NEAR(v.as_measurement().as_scalar<double>(), 0.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathRejectsString)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    EXPECT_THROW(rel::Eval("sin(\"abc\")", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, MathVectorCell)
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
    EXPECT_NEAR(m.as_vector<double>()[0], 0.0, 1e-12);
    EXPECT_NEAR(m.as_vector<double>()[1], 1.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathIntegerVectorPromotesToReal)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    // Integer vector {0, 1, 2} -> sin -> Real vector.
    rel::Value v = rel::Eval("sin({0, 1, 2})", &env);
    ASSERT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kVector);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kReal);
}

TEST(BuiltinFunctionTest, MathMatrixCell)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // sin({{0, PI/2}, {PI, 3*PI/2}}) -> {{0, 1}, {0, -1}} as a Real matrix.
    rel::Value v = rel::Eval("sin({{0, PI/2}, {PI, 3*PI/2}})", &env);
    ASSERT_TRUE(v.is_measurement());
    const xdataset::Measurement& m = v.as_measurement();
    EXPECT_EQ(m.data_kind(), xdataset::DataKind::kMatrix);
    EXPECT_EQ(m.data_type(), xdataset::DataType::kReal);
    EXPECT_EQ(m.shape()[0], 2u);
    EXPECT_EQ(m.shape()[1], 2u);
    EXPECT_NEAR(m.as_matrix<double>()(0, 0), 0.0, 1e-12);
    EXPECT_NEAR(m.as_matrix<double>()(0, 1), 1.0, 1e-12);
    EXPECT_NEAR(m.as_matrix<double>()(1, 0), 0.0, 1e-12);
    EXPECT_NEAR(m.as_matrix<double>()(1, 1), -1.0, 1e-12);
}

TEST(BuiltinFunctionTest, MathOnDatasetVariable)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());  // Vout: 2 rows of Real
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // sin(Vout) preserves the dependency structure (Dependent on freq).
    rel::Value v = rel::Eval("sin(Vout)", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kDependent);
    EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
    EXPECT_EQ(da.data().size(), 2u);
    EXPECT_EQ(da.indep_names().size(), 1u);
    EXPECT_EQ(da.indep_names()[0], "freq");
}

// =========================================================================
//  Coexistence with user-registered functions and expressions
// =========================================================================

TEST(BuiltinFunctionTest, BuiltinsComposeInExpressions)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();
    rel::Environment::InitBuiltinConstants();

    // datasets() returns a DataArray; just confirm it can be stored.
    rel::Value v = rel::Eval("datasets()", &env);
    env.Define("info", v);
    EXPECT_TRUE(env.Get("info").is_data_array());

    // what() can inspect the stored array.
    std::vector<std::string> rows = payload(rel::Eval("what(info)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[1], "Kind: Independent");
}

// =========================================================================
//  Dotted dependent name resolution (e.g. SP.SRC1.i)
// =========================================================================

TEST(DottedDependentTest, SingleSegmentVariableName)
{
    // SP.Vout -- standard single-segment var, should still work.
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("sim");

    // This should resolve as block=SP, var=Vout (original behaviour).
    rel::Value v = rel::Eval("SP.Vout", &env);
    EXPECT_TRUE(v.is_data_array());
}

TEST(DottedDependentTest, DottedDependentVariableFallback)
{
    // SP.SRC1.i -- segments=[SP,SRC1,i].  Block "SP/SRC1" doesn't exist
    // so we fall back to block=SP, var="SRC1.i".
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_dotted_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("sim");

    rel::Value vi = rel::Eval("SP.SRC1.i", &env);
    EXPECT_TRUE(vi.is_data_array());

    rel::Value vv = rel::Eval("SP.SRC1.v", &env);
    EXPECT_TRUE(vv.is_data_array());
}

TEST(DottedDependentTest, IndependentStillWorksWithDottedDependents)
{
    // SP.time -- independent variable, single segment.
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_dotted_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("sim");

    rel::Value v = rel::Eval("SP.time", &env);
    EXPECT_TRUE(v.is_data_array());
}

TEST(DottedDependentTest, DottedDependentViaDDot)
{
    // sim..SRC1.i -- global unique-lookup with dotted name
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_dotted_block_info());
    rel::Environment::AddDataset(std::move(ds));
    // Note: no default dataset set — we use explicit dataset name.

    rel::Value v = rel::Eval("sim..SRC1.i", &env);
    EXPECT_TRUE(v.is_data_array());
}

TEST(DottedDependentTest, UnresolvableDottedVariableReportsError)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_dotted_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("sim");

    EXPECT_THROW({ rel::Eval("SP.nonexistent.var", &env); }, std::runtime_error);
}

TEST(DottedDependentTest, BareDottedVariableResolvesViaUniqueLookup)
{
    // Id.i -- bare 2-segment dotted name resolves directly as a unique DataArray.
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    auto ds = std::unique_ptr<xdataset::Dataset>(new xdataset::Dataset("sim"));
    ds->AddBlock("SP", make_dotted_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("sim");

    rel::Value v = rel::Eval("SRC1.i", &env);
    EXPECT_TRUE(v.is_data_array());

    rel::Value v2 = rel::Eval("SRC1.v", &env);
    EXPECT_TRUE(v2.is_data_array());
}