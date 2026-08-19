// =============================================================================
//  xdataset -- value_test.cc
// =============================================================================
//
//  Tests for the Value class only.

#include "value.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using xdataset::DataKind;
using xdataset::DataType;
using xdataset::Index;
using xdataset::Unit;
using xdataset::VecXd;
using xdataset::VecXi;
using xdataset::VecXcd;
using xdataset::VecXs;
using xdataset::MatXd;
using xdataset::MatXi;
using xdataset::MatXcd;
using xdataset::MatXs;
using rel::Value;

// =========================================================================
//  Construction
// =========================================================================

TEST(ValueTest, DefaultIsMeasurement)
{
    Value v;
    EXPECT_TRUE(v.is_measurement());
    EXPECT_FALSE(v.is_data_array());
}

TEST(ValueTest, ConstructFromMeasurement)
{
    xdataset::Measurement m = xdataset::Measurement::Real(3.14).set_unit(Unit::parse("meter"));
    Value v(m);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.14);
}

TEST(ValueTest, ConstructFromMeasurementInPlace)
{
    Value v = Value::Integer(42);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 42);
}

TEST(ValueTest, ConstructFromDataArray)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 2.0, 3.0});
    auto da = xdataset::DataArray::CreateIndependent(std::move(ds));
    Value v(da);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
}

// =========================================================================
//  Metadata (unified access)
// =========================================================================

TEST(ValueTest, MetadataMeas)
{
    VecXd ev5(5); ev5.setOnes();
    xdataset::Measurement m = xdataset::Measurement::Vector(ev5).set_unit(Unit::parse("Hz"));
    Value v(m);
    EXPECT_EQ(v.data_kind(), DataKind::kVector);
    EXPECT_EQ(v.data_type(), DataType::kReal);
    EXPECT_EQ(v.data_shape(), xdataset::DataShape::Vector(5));
    EXPECT_TRUE(v.unit().same_dimension(Unit::parse("Hz")));
    EXPECT_EQ(v.rows(), 1);
}

TEST(ValueTest, MetadataArray)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({10.0, 20.0, 30.0});
    ds.set_unit(Unit::parse("V"));
    auto da = xdataset::DataArray::CreateIndependent(std::move(ds));
    Value v(da);
    EXPECT_EQ(v.data_kind(), DataKind::kScalar);
    EXPECT_EQ(v.data_type(), DataType::kReal);
    EXPECT_TRUE(v.data_shape().empty());
    EXPECT_TRUE(v.unit().same_dimension(Unit::parse("V")));
    EXPECT_EQ(v.rows(), 3);
}

// =========================================================================
//  Independent DataArray factories (Array*)
// =========================================================================

TEST(ValueTest, ArrayRealFactory)
{
    Value v = Value::ArrayReal({1.0, 2.0, 3.0});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_kind(), DataKind::kScalar);
    EXPECT_EQ(v.data_type(), DataType::kReal);
    EXPECT_EQ(v.rows(), 3);
    const auto& ds = v.as_data_array().data();
    EXPECT_DOUBLE_EQ(ds.scalar_at<double>(0), 1.0);
    EXPECT_DOUBLE_EQ(ds.scalar_at<double>(1), 2.0);
    EXPECT_DOUBLE_EQ(ds.scalar_at<double>(2), 3.0);
}

TEST(ValueTest, ArrayRealFactoryWithUnit)
{
    Value v = Value::ArrayReal({1.0, 2.0}, Unit::parse("V"));
    ASSERT_TRUE(v.is_data_array());
    EXPECT_TRUE(v.unit().same_dimension(Unit::parse("V")));
}

TEST(ValueTest, ArrayIntegerFactory)
{
    Value v = Value::ArrayInteger({4, 5});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_type(), DataType::kInteger);
    EXPECT_EQ(v.rows(), 2);
    EXPECT_EQ(v.as_data_array().data().scalar_at<int>(1), 5);
}

TEST(ValueTest, ArrayComplexFactory)
{
    Value v = Value::ArrayComplex({std::complex<double>(1.0, 2.0),
                                   std::complex<double>(3.0, 4.0)});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_type(), DataType::kComplex);
    auto z = v.as_data_array().data().scalar_at<std::complex<double>>(0);
    EXPECT_DOUBLE_EQ(z.real(), 1.0);
    EXPECT_DOUBLE_EQ(z.imag(), 2.0);
}

TEST(ValueTest, ArrayStringFactory)
{
    Value v = Value::ArrayString({"a", "b"});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_type(), DataType::kString);
    EXPECT_EQ(v.rows(), 2);
    EXPECT_EQ(v.as_data_array().data().scalar_at<std::string>(0), "a");
    EXPECT_EQ(v.as_data_array().data().scalar_at<std::string>(1), "b");
}

TEST(ValueTest, ArrayVectorFactory)
{
    VecXd r0(2); r0 << 1.0, 2.0;
    VecXd r1(2); r1 << 3.0, 4.0;
    Value v = Value::ArrayVector(std::vector<VecXd>{r0, r1});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_kind(), DataKind::kVector);
    EXPECT_EQ(v.data_type(), DataType::kReal);
    EXPECT_EQ(v.rows(), 2);
    const auto& ds = v.as_data_array().data();
    EXPECT_EQ(ds.data_shape(), xdataset::DataShape::Vector(2));
    EXPECT_DOUBLE_EQ(ds.vector_at<double>(0)(0), 1.0);
    EXPECT_DOUBLE_EQ(ds.vector_at<double>(0)(1), 2.0);
    EXPECT_DOUBLE_EQ(ds.vector_at<double>(1)(0), 3.0);
    EXPECT_DOUBLE_EQ(ds.vector_at<double>(1)(1), 4.0);
}

TEST(ValueTest, ArrayVectorStringFactory)
{
    VecXs r0(2); r0(0) = "a"; r0(1) = "b";
    VecXs r1(2); r1(0) = "c"; r1(1) = "d";
    Value v = Value::ArrayVector({r0, r1});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_kind(), DataKind::kVector);
    EXPECT_EQ(v.data_type(), DataType::kString);
    EXPECT_EQ(v.rows(), 2);
    auto row0 = v.as_data_array().data().vector_at<std::string>(0);
    EXPECT_EQ(row0(0), "a");
    EXPECT_EQ(row0(1), "b");
}

TEST(ValueTest, ArrayVectorShapeMismatchThrows)
{
    VecXd r0(2); r0 << 1.0, 2.0;
    VecXd r1(3); r1 << 1.0, 2.0, 3.0;
    EXPECT_THROW(Value::ArrayVector(std::vector<VecXd>{r0, r1}), std::bad_cast);
}

TEST(ValueTest, ArrayMatrixFactory)
{
    MatXd m(2, 2); m << 1.0, 2.0, 3.0, 4.0;
    Value v = Value::ArrayMatrix(std::vector<MatXd>{m});
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.data_kind(), DataKind::kMatrix);
    EXPECT_EQ(v.data_type(), DataType::kReal);
    EXPECT_EQ(v.rows(), 1);
    const auto& ds = v.as_data_array().data();
    EXPECT_EQ(ds.data_shape(), (xdataset::DataShape::Matrix(2, 2)));
    auto mm = ds.matrix_at<double>(0);
    EXPECT_DOUBLE_EQ(mm(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(mm(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(mm(1, 0), 3.0);
    EXPECT_DOUBLE_EQ(mm(1, 1), 4.0);
}
