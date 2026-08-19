// Environment tests powered by GoogleTest.

#include "environment.h"
#include "rel.h"

#include "data_series.h"
#include "dataset.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    // ---- helpers ------------------------------------------------------------

    xdataset::BlockCreateInfo make_block_info()
    {
        xdataset::BlockCreateInfo info;
        // freq: independent, 2 points
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "freq",
                xdataset::DataSeries::CreateScalar<double>(2),
                xdataset::DimensionSpec::Regular(2)});
        // Vout: dependent, scalar
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "Vout",
                xdataset::DataSeries::CreateScalar<double>(2)});
        return info;
    }

} // namespace

// =========================================================================
//  Variables: define / get
// =========================================================================

TEST(EnvironmentTest, DefineAndGet)
{
    rel::Environment env;
    env.Define("x", rel::Value::Real(3.14));
    env.Define("y", rel::Value::Integer(42));

    EXPECT_DOUBLE_EQ(env.Get("x").as_measurement().as_scalar<double>(), 3.14);
    EXPECT_EQ(env.Get("y").as_measurement().as_scalar<int>(), 42);
}

TEST(EnvironmentTest, GetUndefinedReturnsDefault)
{
    rel::Environment env;
    EXPECT_TRUE(env.Get("nonexistent").is_measurement());
}

TEST(EnvironmentTest, RedefineOverwrites)
{
    rel::Environment env;
    env.Define("x", rel::Value::Integer(1));
    env.Define("x", rel::Value::Integer(2));
    EXPECT_EQ(env.Get("x").as_measurement().as_scalar<int>(), 2);
}

// =========================================================================
//  Built-in constants
// =========================================================================

TEST(EnvironmentTest, BuiltinPi)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("PI");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinLowercasePi)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("pi");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinE)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("e");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 2.718281822);
}

TEST(EnvironmentTest, BuiltinBoltzmann)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("boltzmann");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 1.380658e-23);
}

TEST(EnvironmentTest, BuiltinTinyReal)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("tinyReal");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 2.2e-308);
}

// =========================================================================
//  Dataset management
// =========================================================================

TEST(EnvironmentTest, AddDatasetAndSetDefault)
{
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());

    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    ASSERT_NE(rel::Environment::DefaultDataset(), nullptr);
    EXPECT_EQ(rel::Environment::DefaultDataset()->name(), "noise");
}

TEST(EnvironmentTest, DefaultDatasetNullWhenNotSet)
{
    EXPECT_EQ(rel::Environment::DefaultDataset(), nullptr);
}

// =========================================================================
//  Variable / constant lookup
// =========================================================================

TEST(EnvironmentTest, LookupVariableOrConstantVariable)
{
    rel::Environment env;
    env.Define("x", rel::Value::Real(1.5));

    const rel::Value* v = env.LookupVariableOrConstant("x");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 1.5);
}

TEST(EnvironmentTest, LookupVariableOrConstantConstant)
{
    rel::Environment::InitBuiltinConstants();
    rel::Environment env;
    const rel::Value* v = env.LookupVariableOrConstant("PI");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
}

TEST(EnvironmentTest, LookupVariableOrConstantNotFound)
{
    rel::Environment env;
    EXPECT_EQ(env.LookupVariableOrConstant("no_such_var"), nullptr);
}

// =========================================================================
//  Dataset lookup
// =========================================================================

TEST(EnvironmentTest, FindDatasetFound)
{
    auto ds = std::make_unique<xdataset::Dataset>("noise");
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));

    xdataset::Dataset* found = rel::Environment::FindDataset("noise");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name(), "noise");
}

TEST(EnvironmentTest, FindDatasetNotFound)
{
    EXPECT_EQ(rel::Environment::FindDataset("nonexistent"), nullptr);
}
