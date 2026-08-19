// =============================================================================
//  xdataset -- operation_test.cc
// =============================================================================
//
//  Tests for the public Operation* API.

#include "operation/operator.h"
#include "block_fixtures.h"

#include <complex>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using xdataset::Block;
using xdataset::DataArray;
using xdataset::DataArrayKind;
using xdataset::DataKind;
using xdataset::DataType;
using xdataset::Index;
using xdataset::Unit;
using rel::Value;
using xdataset::VecXd;
using xdataset::VecXi;
using xdataset::VecXcd;
using xdataset::VecXs;
using xdataset::MatXd;
using xdataset::MatXi;
using xdataset::MatXcd;
using xdataset::MatXs;
using xdataset::block_fixtures::MakeBaseCreateInfo;

using rel::operation::OperationAdd;
using rel::operation::OperationSub;
using rel::operation::OperationMul;
using rel::operation::OperationDiv;
using rel::operation::OperationTimes;
using rel::operation::OperationRdivide;
using rel::operation::OperationMod;
using rel::operation::OperationPow;
using rel::operation::OperationNegate;
using rel::operation::OperationNot;
using rel::operation::OperationBitNot;
using rel::operation::OperationEq;
using rel::operation::OperationNeq;
using rel::operation::OperationLt;
using rel::operation::OperationGt;
using rel::operation::OperationLe;
using rel::operation::OperationGe;
using rel::operation::OperationAnd;
using rel::operation::OperationOr;
using rel::operation::OperationBitAnd;
using rel::operation::OperationBitOr;
using rel::operation::OperationBitXor;
using rel::operation::OperationShl;
using rel::operation::OperationShr;
using rel::operation::OperationConditional;
using rel::operation::OperationIf;
using rel::operation::OperationMatrix;
using rel::operation::OperationSweep;

#define EXPECT_MEAS_SCALAR_DOUBLE(val, expected) do { \
    ASSERT_TRUE((val).is_measurement()); \
    EXPECT_DOUBLE_EQ((val).as_measurement().as_scalar<double>(), (expected)); \
} while(0)

// =========================================================================
//  OperationAdd / OperationSub
// =========================================================================

TEST(OperationAddTest, MeasMeasScalarScalar)
{
    Value v1 = Value::Real(3.0);
    Value v2 = Value::Real(4.0);
    Value result = OperationAdd(v1, v2);
    EXPECT_MEAS_SCALAR_DOUBLE(result, 7.0);
}

TEST(OperationSubTest, MeasMeasScalarScalar)
{
    Value v1 = Value::Real(10.0);
    Value v2 = Value::Real(3.0);
    Value result = OperationSub(v1, v2);
    EXPECT_MEAS_SCALAR_DOUBLE(result, 7.0);
}

TEST(OperationAddTest, MeasMeasScalarVectorBroadcast)
{
    Value v1 = Value::Real(2.0);
    VecXd ev(3); ev << 1.0, 2.0, 3.0;
    Value v2 = Value::Vector(ev);
    Value result = OperationAdd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 3.0);
    EXPECT_DOUBLE_EQ(vec(1), 4.0);
    EXPECT_DOUBLE_EQ(vec(2), 5.0);
}

TEST(OperationAddTest, MeasMeasVectorVector)
{
    VecXd a(3); a << 1.0, 2.0, 3.0;
    VecXd b(3); b << 4.0, 5.0, 6.0;
    Value result = OperationAdd(Value::Vector(a), Value::Vector(b));
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 5.0);
    EXPECT_DOUBLE_EQ(vec(1), 7.0);
    EXPECT_DOUBLE_EQ(vec(2), 9.0);
}

TEST(OperationAddTest, MeasMeasIntAndRealPromoteToReal)
{
    Value v1 = Value::Integer(3);
    Value v2 = Value::Real(4.5);
    Value result = OperationAdd(v1, v2);
    EXPECT_MEAS_SCALAR_DOUBLE(result, 7.5);
}

// ---- Meas x Array -------------------------------------------------------

TEST(OperationAddTest, MeasScalarArrayScalar)
{
    Value v1 = Value::Real(5.0);
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 2.0, 3.0});
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds)));
    Value result = OperationAdd(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 6.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 7.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(2), 8.0);
}

TEST(OperationAddTest, ArrayScalarMeasScalar)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 2.0, 3.0});
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds)));
    Value v2 = Value::Real(5.0);
    Value result = OperationAdd(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 6.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 7.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(2), 8.0);
}

TEST(OperationAddTest, ArrayArrayScalarSameRows)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 2.0, 3.0});
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({10.0, 20.0, 30.0});
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds1)));
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationAdd(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 11.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 22.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(2), 33.0);
}

// =========================================================================
//  OperationMul
// =========================================================================

TEST(OperationMulTest, MeasMeasScalarScalar)
{
    Value result = OperationMul(Value::Real(3.0), Value::Real(4.0));
    EXPECT_MEAS_SCALAR_DOUBLE(result, 12.0);
}

TEST(OperationMulTest, MeasMeasVectorxMatrix)
{
    VecXd v(2); v << 1.0, 2.0;
    MatXd m(2, 3);
    m << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
    Value result = OperationMul(Value::Vector(v), Value::Matrix(m));
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 9.0);
    EXPECT_DOUBLE_EQ(vec(1), 12.0);
    EXPECT_DOUBLE_EQ(vec(2), 15.0);
}

TEST(OperationMulTest, MatrixMulByScalarIsElementwise)
{
    MatXd m(2, 2); m << 1.0, 2.0, 3.0, 4.0;
    Value result = OperationMul(Value::Matrix(m), Value::Real(2.0));
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 8.0);
}

TEST(OperationMulTest, ArrayArrayScalarSameRows)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({2.0, 3.0});
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({4.0, 5.0});
    Value result = OperationMul(
        Value(xdataset::DataArray::CreateIndependent(std::move(ds1))),
        Value(xdataset::DataArray::CreateIndependent(std::move(ds2))));
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 8.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 15.0);
}

// =========================================================================
//  OperationDiv
// =========================================================================

TEST(OperationDivTest, MeasMeasScalarScalar)
{
    Value result = OperationDiv(Value::Real(12.0), Value::Real(4.0));
    EXPECT_MEAS_SCALAR_DOUBLE(result, 3.0);
}

TEST(OperationDivTest, ScalarDivByZeroThrows)
{
    EXPECT_THROW(OperationDiv(Value::Real(1.0), Value::Real(0.0)),
                 std::runtime_error);
}

TEST(OperationDivTest, MeasMeasVectorDivMatrix)
{
    VecXd v(2); v << 6.0, 8.0;
    MatXd m(2, 2); m << 2.0, 0.0, 0.0, 4.0;
    Value result = OperationDiv(Value::Vector(v), Value::Matrix(m));
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 3.0);
    EXPECT_DOUBLE_EQ(vec(1), 2.0);
}

// =========================================================================
//  OperationTimes / OperationRdivide (element-wise, broadcast)
// =========================================================================

TEST(OperationTimesTest, MatrixMatrixElementwise)
{
    MatXd a(2, 2); a << 1.0, 2.0, 3.0, 4.0;
    MatXd b(2, 2); b << 10.0, 20.0, 30.0, 40.0;
    Value result = OperationTimes(Value::Matrix(a), Value::Matrix(b));
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 10.0);
    EXPECT_DOUBLE_EQ(mat(0, 1), 40.0);
    EXPECT_DOUBLE_EQ(mat(1, 0), 90.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 160.0);
}

TEST(OperationTimesTest, MatrixTimesScalarBroadcast)
{
    MatXd m(2, 2); m << 1.0, 2.0, 3.0, 4.0;
    Value result = OperationTimes(Value::Matrix(m), Value::Real(2.0));
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 8.0);
}

TEST(OperationTimesTest, IntVectorElementwisePromote)
{
    VecXi a(3); a << 2, 3, 4;
    VecXi b(3); b << 5, 6, 7;
    Value result = OperationTimes(Value::Vector(a), Value::Vector(b));
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 10);
    EXPECT_EQ(vec(1), 18);
    EXPECT_EQ(vec(2), 28);
}

TEST(OperationRdivideTest, MatrixMatrixElementwise)
{
    MatXd a(2, 2); a << 6.0, 8.0, 10.0, 12.0;
    MatXd b(2, 2); b << 2.0, 4.0, 5.0, 3.0;
    Value result = OperationRdivide(Value::Matrix(a), Value::Matrix(b));
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 3.0);
    EXPECT_DOUBLE_EQ(mat(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(mat(1, 0), 2.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 4.0);
}

TEST(OperationRdivideTest, VectorVectorElementwise)
{
    VecXd a(3); a << 6.0, 8.0, 10.0;
    VecXd b(3); b << 2.0, 4.0, 5.0;
    Value result = OperationRdivide(Value::Vector(a), Value::Vector(b));
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 3.0);
    EXPECT_DOUBLE_EQ(vec(1), 2.0);
    EXPECT_DOUBLE_EQ(vec(2), 2.0);
}

TEST(OperationRdivideTest, IntVectorPromoteToReal)
{
    VecXi a(2); a << 3, 8;
    VecXi b(2); b << 2, 4;
    Value result = OperationRdivide(Value::Vector(a), Value::Vector(b));
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 1.5);
    EXPECT_DOUBLE_EQ(vec(1), 2.0);
}

// =========================================================================
//  OperationMod
// =========================================================================

TEST(OperationModTest, MeasMeasIntScalarScalar)
{
    Value result = OperationMod(Value::Integer(10), Value::Integer(3));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 1);
}

TEST(OperationModTest, MeasMeasDoubleScalarScalar)
{
    Value result = OperationMod(Value::Real(10.5), Value::Real(3.0));
    EXPECT_MEAS_SCALAR_DOUBLE(result, 1.5);
}

TEST(OperationModTest, ScalarModByZeroThrows)
{
    EXPECT_THROW(OperationMod(Value::Integer(10), Value::Integer(0)),
                 std::runtime_error);
}

// =========================================================================
//  OperationPow
// =========================================================================

TEST(OperationPowTest, MeasMeasScalarScalar)
{
    Value result = OperationPow(Value::Real(2.0), Value::Real(3.0));
    EXPECT_MEAS_SCALAR_DOUBLE(result, 8.0);
}

TEST(OperationPowTest, MeasMeasScalarVectorBroadcast)
{
    Value v1 = Value::Real(2.0);
    VecXd ev(3); ev << 1.0, 2.0, 3.0;
    Value v2 = Value::Vector(ev);
    Value result = OperationPow(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 2.0);
    EXPECT_DOUBLE_EQ(vec(1), 4.0);
    EXPECT_DOUBLE_EQ(vec(2), 8.0);
}

// =========================================================================
//  OperationNegate
// =========================================================================

TEST(OperationNegateTest, MeasScalar)
{
    Value result = OperationNegate(Value::Real(5.0));
    EXPECT_MEAS_SCALAR_DOUBLE(result, -5.0);
}

TEST(OperationNegateTest, ArrayScalar)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, -2.0, 3.0});
    Value v(xdataset::DataArray::CreateIndependent(std::move(ds)));
    Value result = OperationNegate(v);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), -1.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 2.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(2), -3.0);
}

// =========================================================================
//  OperationNot
// =========================================================================

TEST(OperationNotTest, MeasScalarZero)
{
    Value result = OperationNot(Value::Real(0.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationNotTest, MeasScalarNonZero)
{
    Value result = OperationNot(Value::Real(3.5));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

// =========================================================================
//  OperationBitNot
// =========================================================================

TEST(OperationBitNotTest, MeasScalar)
{
    Value result = OperationBitNot(Value::Integer(0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), ~0);
}

// =========================================================================
//  OperationEq / OperationNeq / OperationLt / OperationGt
// =========================================================================

TEST(OperationEqTest, MeasMeasScalarEqual)
{
    Value result = OperationEq(Value::Real(3.0), Value::Real(3.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationEqTest, MeasMeasScalarNotEqual)
{
    Value result = OperationEq(Value::Real(3.0), Value::Real(4.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

TEST(OperationNeqTest, MeasMeasScalar)
{
    Value result = OperationNeq(Value::Real(3.0), Value::Real(4.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationLtTest, MeasMeasScalar)
{
    Value result = OperationLt(Value::Real(2.0), Value::Real(5.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationGtTest, MeasMeasScalar)
{
    Value result = OperationGt(Value::Real(5.0), Value::Real(2.0));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// ---- Cmp: int (eq/ne/lt/gt/le/ge) --------------------------------------

TEST(OperationLtTest, IntScalar)
{
    Value v1 = Value::Integer(3);
    Value v2 = Value::Integer(7);
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationGtTest, IntScalar)
{
    Value v1 = Value::Integer(7);
    Value v2 = Value::Integer(3);
    Value result = OperationGt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationLeTest, IntScalar)
{
    Value v1 = Value::Integer(3);
    Value v2 = Value::Integer(3);
    Value result = OperationLe(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// ---- Cmp: complex (abs() for < <= > >=) --------------------------------

TEST(OperationEqTest, ComplexEqual)
{
    Value v1 = Value::Complex(std::complex<double>(1.0, 2.0));
    Value v2 = Value::Complex(std::complex<double>(1.0, 2.0));
    Value result = OperationEq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationNeqTest, ComplexNotEqual)
{
    Value v1 = Value::Complex(std::complex<double>(1.0, 2.0));
    Value v2 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value result = OperationNeq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationLtTest, ComplexAbs)
{
    // |3+4i| = 5  <  |6+0i| = 6
    Value v1 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value v2 = Value::Complex(std::complex<double>(6.0, 0.0));
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationGtTest, ComplexAbs)
{
    // |5+0i| = 5  >  |3+4i| = 5  �?false (equal abs)
    Value v1 = Value::Complex(std::complex<double>(5.0, 0.0));
    Value v2 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value result = OperationGt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

TEST(OperationLeTest, ComplexAbs)
{
    Value v1 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value v2 = Value::Complex(std::complex<double>(5.0, 0.0));
    Value result = OperationLe(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);  // 5 <= 5
}

// ---- Cmp: string -------------------------------------------------------

TEST(OperationEqTest, StringEqual)
{
    Value v1 = Value::String("abc");
    Value v2 = Value::String("abc");
    Value result = OperationEq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationNeqTest, StringNotEqual)
{
    Value v1 = Value::String("abc");
    Value v2 = Value::String("xyz");
    Value result = OperationNeq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationLtTest, StringLex)
{
    Value v1 = Value::String("abc");
    Value v2 = Value::String("xyz");
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationGtTest, StringLex)
{
    Value v1 = Value::String("xyz");
    Value v2 = Value::String("abc");
    Value result = OperationGt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// ---- Cmp: mixed types (int↔double compare directly) ---------------------

TEST(OperationEqTest, IntAndDouble)
{
    Value v1 = Value::Integer(3);
    Value v2 = Value::Real(3.0);
    Value result = OperationEq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

TEST(OperationLtTest, IntAndDouble)
{
    Value v1 = Value::Integer(3);
    Value v2 = Value::Real(5.5);
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// ---- Cmp: mixed complex↔real (promote to complex, use abs() for < >) -----

TEST(OperationLtTest, ComplexAndReal)
{
    // real 5.0 becomes (5,0), |3+4i|=5, |5+0i|=5 �?not less
    Value v1 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value v2 = Value::Real(5.0);
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);  // 5 < 5 �?false
}

// ---- Cmp: row broadcast (Array x Array) ---------------------------------

TEST(OperationEqTest, ArrayArrayScalarSameRows)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 3.0, 5.0});
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 3.0, 5.0});
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds1)));
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationEq(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.scalar_at<int>(0), 1);
    EXPECT_EQ(arr.scalar_at<int>(1), 1);
    EXPECT_EQ(arr.scalar_at<int>(2), 1);
}

TEST(OperationLtTest, ArrayArrayBroadcastRows)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({3.0});      // 1 row
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 5.0});  // 2 rows
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds1)));
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationLt(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr.scalar_at<int>(0), 0);  // 3 < 1 �?0
    EXPECT_EQ(arr.scalar_at<int>(1), 1);  // 3 < 5 �?1
}

// ---- Cmp: cell broadcast (Vector broadcast in Meas) ---------------------

TEST(OperationEqTest, MeasVectorMeasScalarBroadcast)
{
    VecXd a(3); a << 2.0, 2.0, 2.0;
    Value v1 = Value::Vector(a);
    Value v2 = Value::Real(2.0);
    Value result = OperationEq(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 1); EXPECT_EQ(vec(1), 1); EXPECT_EQ(vec(2), 1);
}

// ---- Logic: row broadcast (Array x Array) --------------------------------

TEST(OperationAndTest, ArrayArrayBroadcastRows)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0});     // 1 row, non-zero
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({0.0, 3.0}); // 2 rows
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds1)));
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr.scalar_at<int>(0), 0);  // 1 && 0 = 0
    EXPECT_EQ(arr.scalar_at<int>(1), 1);  // 1 && 1 = 1
}

// ---- Not: array ------------------------------------------------

TEST(OperationNotTest, ArrayScalar)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<double>({0.0, 5.0, -3.0});
    Value v(xdataset::DataArray::CreateIndependent(std::move(ds)));
    Value result = OperationNot(v);
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.scalar_at<int>(0), 1);
    EXPECT_EQ(arr.scalar_at<int>(1), 0);
    EXPECT_EQ(arr.scalar_at<int>(2), 0);
}

// =========================================================================
//  OperationAnd / OperationOr
// =========================================================================

TEST(OperationAndTest, MeasMeasScalarScalar)
{
    Value v1 = Value::Real(0.0);
    Value v2 = Value::Real(1.0);
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

TEST(OperationAndTest, BoolOperands)
{
    Value v1 = Value::Boolean(true);
    Value v2 = Value::Boolean(false);
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

TEST(OperationOrTest, BoolOperands)
{
    Value v1 = Value::Boolean(true);
    Value v2 = Value::Boolean(false);
    Value result = OperationOr(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// ---- Logic: real/complex operands (as_logical: non-zero�?) --------------

TEST(OperationAndTest, RealOperands)
{
    Value v1 = Value::Real(3.5);
    Value v2 = Value::Real(0.0);
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);  // 1 && 0 = 0
}

TEST(OperationOrTest, RealOperands)
{
    Value v1 = Value::Real(0.0);
    Value v2 = Value::Real(-2.0);
    Value result = OperationOr(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);  // 0 || 1 = 1
}

TEST(OperationAndTest, ComplexOperands)
{
    // (1+0i) non-zero �?1, (0+0i) zero �?0
    Value v1 = Value::Complex(std::complex<double>(1.0, 0.0));
    Value v2 = Value::Complex(std::complex<double>(0.0, 0.0));
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);  // 1 && 0 = 0
}

TEST(OperationOrTest, ComplexOperands)
{
    // (0+5i) non-zero �?1
    Value v1 = Value::Complex(std::complex<double>(0.0, 5.0));
    Value v2 = Value::Complex(std::complex<double>(0.0, 0.0));
    Value result = OperationOr(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);  // 1 || 0 = 1
}

// ---- Logic: vector operands (as_logical broadcasts) ---------------------

TEST(OperationAndTest, VectorOperands)
{
    VecXd a(2); a << 0.0, 1.0;
    VecXd b(2); b << 2.0, 0.0;
    Value v1 = Value::Vector(a);
    Value v2 = Value::Vector(b);
    Value result = OperationAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 0);  // 0 && 1
    EXPECT_EQ(vec(1), 0);  // 1 && 0
}

// ---- Logic: Not with real/complex --------------------------------------

TEST(OperationNotTest, RealOperand)
{
    Value result = OperationNot(Value::Real(3.5));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);  // !1 = 0
}

TEST(OperationNotTest, ComplexOperand)
{
    // (0+5i) non-zero �?!1 = 0
    Value result = OperationNot(Value::Complex(std::complex<double>(0.0, 5.0)));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), false);
}

TEST(OperationNotTest, BoolOperand)
{
    Value result = OperationNot(Value::Boolean(false));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<bool>(), true);
}

// =========================================================================
//  OperationBitAnd / OperationBitOr / OperationBitXor
// =========================================================================

TEST(OperationBitAndTest, MeasMeasScalar)
{
    Value v1 = Value::Integer(6);   // 110
    Value v2 = Value::Integer(3);   // 011
    Value result = OperationBitAnd(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 2);  // 010
}

TEST(OperationBitOrTest, MeasMeasScalar)
{
    Value v1 = Value::Integer(6);
    Value v2 = Value::Integer(3);
    Value result = OperationBitOr(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 7);
}

TEST(OperationBitXorTest, MeasMeasScalar)
{
    Value v1 = Value::Integer(6);
    Value v2 = Value::Integer(3);
    Value result = OperationBitXor(v1, v2);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 5);
}

// =========================================================================
//  OperationShl / OperationShr
// =========================================================================

TEST(OperationShlTest, MeasMeasScalar)
{
    Value result = OperationShl(Value::Integer(1), Value::Integer(3));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 8);
}

TEST(OperationShrTest, MeasMeasScalar)
{
    Value result = OperationShr(Value::Integer(16), Value::Integer(2));
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 4);
}

// =========================================================================
//  OperationMatrix
// =========================================================================

TEST(OperationMatrixTest, IntAndRealPromote)
{
    Value result = OperationMatrix({Value::Integer(1), Value::Real(2.5)});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 1.0);
    EXPECT_DOUBLE_EQ(vec(1), 2.5);
}

TEST(OperationMatrixTest, StringScalarsToVector)
{
    Value v1 = Value::String("hello");
    Value v2 = Value::String("world");
    Value result = OperationMatrix({v1, v2});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<std::string>();
    EXPECT_EQ(vec(0), "hello");
    EXPECT_EQ(vec(1), "world");
}

TEST(OperationMatrixTest, SameUnit)
{
    Unit u = Unit::parse("V");
    Value result = OperationMatrix({Value::Real(1.0, u), Value::Real(2.0, u)});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_TRUE(result.as_measurement().unit().same_dimension(u));
}

TEST(OperationMatrixTest, IncompatibleUnitsThrows)
{
    Unit uv = Unit::parse("V");
    Unit ua = Unit::parse("A");
    EXPECT_THROW(OperationMatrix({Value::Real(1.0, uv), Value::Real(2.0, ua)}),
                 std::runtime_error);
}

TEST(OperationMatrixTest, EmptyThrows)
{
    EXPECT_THROW(OperationMatrix({}), std::runtime_error);
}

TEST(OperationMatrixTest, DataArraysSameKindSameShape)
{
    auto ds1 = xdataset::DataSeries::CreateScalarFromVector<double>({1.0, 2.0});
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({3.0, 4.0});
    Value v1(xdataset::DataArray::CreateIndependent(std::move(ds1)));
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationMatrix({v1, v2});
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.size(), 2u);
}

TEST(OperationMatrixTest, PreservesFirstDataArrayMetadata)
{
    Block block(MakeBaseCreateInfo());
    DataArray da_z = block.GetOrCreateDataArray("z");
    Value v1(std::move(da_z));
    auto ds2 = xdataset::DataSeries::CreateScalarFromVector<double>({3.0});
    Value v2(xdataset::DataArray::CreateIndependent(std::move(ds2)));
    Value result = OperationMatrix({v1, v2});
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array();
    EXPECT_EQ(arr.multi_dimension_spec().rank(), 2u);
    EXPECT_EQ(arr.data_kind(), DataArrayKind::kDependent);
}

TEST(OperationMatrixTest, TwoScalarsStayVector)
{
    Value result = OperationMatrix({Value::Integer(1), Value::Integer(2)});
    ASSERT_TRUE(result.is_measurement());
    ASSERT_EQ(result.as_measurement().data_kind(), DataKind::kVector);
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec(0), 1);
    EXPECT_EQ(vec(1), 2);
}

// =========================================================================
//  OperationSweep
// =========================================================================

TEST(OperationSweepTest, ScalarOnly)
{
    Value v1 = Value::Real(1.0);
    Value v2 = Value::Real(2.0);
    Value v3 = Value::Real(3.0);
    Value result = OperationSweep({v1, v2, v3});
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 1.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 2.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(2), 3.0);
}

TEST(OperationSweepTest, VectorOnly)
{
    VecXd a(2); a << 1.0, 2.0;
    VecXd b(2); b << 3.0, 4.0;
    Value result = OperationSweep({Value::Vector(a), Value::Vector(b)});
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_DOUBLE_EQ(arr.vector_at<double>(0)(0), 1.0);
    EXPECT_DOUBLE_EQ(arr.vector_at<double>(0)(1), 2.0);
}

TEST(OperationSweepTest, IntAndRealPromote)
{
    Value result = OperationSweep({Value::Integer(1), Value::Real(2.5)});
    ASSERT_TRUE(result.is_data_array());
    const auto& arr = result.as_data_array().data();
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(0), 1.0);
    EXPECT_DOUBLE_EQ(arr.scalar_at<double>(1), 2.5);
}

TEST(OperationSweepTest, EmptyThrows)
{
    EXPECT_THROW(OperationSweep({}), std::runtime_error);
}

TEST(OperationSweepTest, IncompatibleUnitsThrows)
{
    Unit uv = Unit::parse("V");
    Unit ua = Unit::parse("A");
    EXPECT_THROW(OperationSweep({Value::Real(1.0, uv), Value::Real(2.0, ua)}),
                 std::runtime_error);
}

// =========================================================================
//  OperationConditional
// =========================================================================

// ---- Scalar: basic ternary -----------------------------------------------

TEST(OperationConditionalTest, ScalarTruePath)
{
    Value cond = Value::Integer(1);
    Value t = Value::Real(10.0);
    Value f = Value::Real(20.0);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 10.0);
}

TEST(OperationConditionalTest, ScalarFalsePath)
{
    Value cond = Value::Integer(0);
    Value t = Value::Real(10.0);
    Value f = Value::Real(20.0);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 20.0);
}

TEST(OperationConditionalTest, ScalarNegativeCondition)
{
    // non-zero (negative) �?true path
    Value cond = Value::Integer(-3);
    Value t = Value::Integer(100);
    Value f = Value::Integer(200);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 100);
}

// ---- Bool condition ------------------------------------------------------

TEST(OperationConditionalTest, BoolConditionTrue)
{
    Value cond = Value::Boolean(true);
    Value t = Value::Integer(42);
    Value f = Value::Integer(99);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 42);
}

TEST(OperationConditionalTest, BoolConditionFalse)
{
    Value cond = Value::Boolean(false);
    Value t = Value::Integer(42);
    Value f = Value::Integer(99);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 99);
}

// ---- Scalar condition, vector operands (broadcast) -----------------------

TEST(OperationConditionalTest, ScalarCondVectorTF)
{
    Value cond = Value::Integer(1);
    VecXd tv(3); tv << 10.0, 20.0, 30.0;
    VecXd fv(3); fv << 1.0, 2.0, 3.0;
    Value t = Value::Vector(tv);
    Value f = Value::Vector(fv);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 10.0);
    EXPECT_DOUBLE_EQ(vec(1), 20.0);
    EXPECT_DOUBLE_EQ(vec(2), 30.0);
}

// ---- Vector condition, scalar true/false ---------------------------------

TEST(OperationConditionalTest, VectorCondScalarTF)
{
    VecXd cv(4); cv << 1.0, 0.0, 1.0, 0.0;
    Value cond = Value::Vector(cv);
    Value t = Value::Integer(10);
    Value f = Value::Integer(20);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 10);
    EXPECT_EQ(vec(1), 20);
    EXPECT_EQ(vec(2), 10);
    EXPECT_EQ(vec(3), 20);
}

// ---- Vector condition, vector true/false (element-wise) ------------------

TEST(OperationConditionalTest, VectorElementWise)
{
    VecXd cv(3); cv << 1.0, 0.0, 2.0;  // non-zero counts as true
    VecXd tv(3); tv << 100.0, 200.0, 300.0;
    VecXd fv(3); fv << -1.0, -2.0, -3.0;
    Value cond = Value::Vector(cv);
    Value t = Value::Vector(tv);
    Value f = Value::Vector(fv);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 100.0);  // 1 �?true
    EXPECT_DOUBLE_EQ(vec(1), -2.0);   // 0 �?false
    EXPECT_DOUBLE_EQ(vec(2), 300.0);  // 2 �?true
}

// ---- Matrix condition (scalar tf broadcast) ------------------------------

TEST(OperationConditionalTest, MatrixCondScalarTF)
{
    MatXd cm(2, 2); cm << 1.0, 0.0, 0.0, 1.0;
    Value cond = Value::Matrix(cm);
    Value t = Value::Integer(99);
    Value f = Value::Integer(-1);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<int>();
    EXPECT_EQ(mat(0, 0), 99);
    EXPECT_EQ(mat(0, 1), -1);
    EXPECT_EQ(mat(1, 0), -1);
    EXPECT_EQ(mat(1, 1), 99);
}

// ---- Matrix × Matrix element-wise ----------------------------------------

TEST(OperationConditionalTest, MatrixElementWise)
{
    MatXd cm(2, 2); cm << 1.0, 0.0, 3.0, 0.0;
    MatXd tm(2, 2); tm << 10.0, 20.0, 30.0, 40.0;
    MatXd fm(2, 2); fm << 1.0, 2.0, 3.0, 4.0;
    Value cond = Value::Matrix(cm);
    Value t = Value::Matrix(tm);
    Value f = Value::Matrix(fm);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 10.0);
    EXPECT_DOUBLE_EQ(mat(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(mat(1, 0), 30.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 4.0);
}

// ---- Complex operands ----------------------------------------------------

TEST(OperationConditionalTest, ComplexOperands)
{
    Value cond = Value::Integer(1);
    Value t = Value::Complex(std::complex<double>(1.0, 2.0));
    Value f = Value::Complex(std::complex<double>(3.0, 4.0));
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto c = result.as_measurement().as_scalar<std::complex<double>>();
    EXPECT_DOUBLE_EQ(c.real(), 1.0);
    EXPECT_DOUBLE_EQ(c.imag(), 2.0);
}

// ---- Type promotion (int + real �?real) ----------------------------------

TEST(OperationConditionalTest, TypePromotion)
{
    Value cond = Value::Integer(1);
    Value t = Value::Integer(1);     // int
    Value f = Value::Real(2.5);      // real �?promotes to real
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    // Should promote to double
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 1.0);
}

// ---- Unit propagation (from true/false, not condition) -------------------

TEST(OperationConditionalTest, UnitPropagation)
{
    Unit uv = Unit::parse("V");
    Value cond = Value::Integer(1);
    Value t = Value::Real(10.0, uv);
    Value f = Value::Real(20.0, uv);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_TRUE(result.unit().same_dimension(uv));
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 10.0);
}

TEST(OperationConditionalTest, UnitMismatchThrows)
{
    Unit uv = Unit::parse("V");
    Unit ua = Unit::parse("A");
    Value cond = Value::Integer(1);
    Value t = Value::Real(10.0, uv);
    Value f = Value::Real(20.0, ua);
    EXPECT_THROW(OperationConditional(cond, t, f), std::runtime_error);
}

// ---- String operands -----------------------------------------------------

TEST(OperationConditionalTest, StringScalar)
{
    Value cond = Value::Integer(1);
    Value t = Value::String("yes");
    Value f = Value::String("no");
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<std::string>(), "yes");
}

TEST(OperationConditionalTest, StringFalsePath)
{
    Value cond = Value::Integer(0);
    Value t = Value::String("yes");
    Value f = Value::String("no");
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<std::string>(), "no");
}

TEST(OperationConditionalTest, StringVectorElementWise)
{
    VecXd cv(3); cv << 1.0, 0.0, 1.0;
    Value cond = Value::Vector(cv);
    VecXs tv(3); tv(0) = "a"; tv(1) = "b"; tv(2) = "c";
    VecXs fv(3); fv(0) = "x"; fv(1) = "y"; fv(2) = "z";
    Value t = Value::Vector(tv);
    Value f = Value::Vector(fv);
    Value result = OperationConditional(cond, t, f);
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<std::string>();
    EXPECT_EQ(vec(0), "a");
    EXPECT_EQ(vec(1), "y");
    EXPECT_EQ(vec(2), "c");
}

TEST(OperationConditionalTest, StringMixedNumericThrows)
{
    Value cond = Value::Integer(1);
    Value t = Value::String("s");
    Value f = Value::Integer(42);
    EXPECT_THROW(OperationConditional(cond, t, f), std::runtime_error);
}

// =========================================================================
//  OperationIf
// =========================================================================

TEST(OperationIfTest, ThreeOperandsEquivalentToConditionalTrue)
{
    // If(cond, true_val, else_val) === Conditional(cond, true_val, else_val)
    Value cond = Value::Integer(1);
    Value t = Value::Integer(42);
    Value f = Value::Integer(99);
    Value result = OperationIf({cond, t, f});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 42);
}

TEST(OperationIfTest, ThreeOperandsEquivalentToConditionalFalse)
{
    Value cond = Value::Integer(0);
    Value t = Value::Integer(42);
    Value f = Value::Integer(99);
    Value result = OperationIf({cond, t, f});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 99);
}

TEST(OperationIfTest, FiveOperandsFirstBranchMatches)
{
    // If(cond1, val1, cond2, val2, else)
    Value c1 = Value::Integer(1);   // true
    Value v1 = Value::Integer(10);
    Value c2 = Value::Integer(0);   // false
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 10);
}

TEST(OperationIfTest, FiveOperandsSecondBranchMatches)
{
    Value c1 = Value::Integer(0);   // false
    Value v1 = Value::Integer(10);
    Value c2 = Value::Integer(1);   // true
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 20);
}

TEST(OperationIfTest, FiveOperandsNoBranchMatches)
{
    Value c1 = Value::Integer(0);   // false
    Value v1 = Value::Integer(10);
    Value c2 = Value::Integer(0);   // false
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 99);
}

TEST(OperationIfTest, SevenOperandsThirdBranchMatches)
{
    // If(c1,v1, c2,v2, c3,v3, else)
    Value c1 = Value::Integer(0);   // false
    Value v1 = Value::Integer(10);
    Value c2 = Value::Integer(0);   // false
    Value v2 = Value::Integer(20);
    Value c3 = Value::Integer(1);   // true
    Value v3 = Value::Integer(30);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, c3, v3, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 30);
}

TEST(OperationIfTest, FirstTrueTakesPriority)
{
    // Both conditions true, first one wins
    Value c1 = Value::Integer(1);
    Value v1 = Value::Integer(10);
    Value c2 = Value::Integer(1);
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 10);
}

TEST(OperationIfTest, VectorElementWise)
{
    VecXd cv1(3); cv1 << 1.0, 0.0, 1.0;
    VecXd cv2(3); cv2 << 0.0, 1.0, 0.0;
    Value c1 = Value::Vector(cv1);
    Value v1 = Value::Integer(10);
    Value c2 = Value::Vector(cv2);
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 10);  // c1 true
    EXPECT_EQ(vec(1), 20);  // c2 true
    EXPECT_EQ(vec(2), 10);  // c1 true
}

TEST(OperationIfTest, VectorElementWiseElseCase)
{
    VecXd cv1(3); cv1 << 0.0, 0.0, 0.0;
    VecXd cv2(3); cv2 << 0.0, 0.0, 0.0;
    Value c1 = Value::Vector(cv1);
    Value v1 = Value::Integer(10);
    Value c2 = Value::Vector(cv2);
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<int>();
    EXPECT_EQ(vec(0), 99);
    EXPECT_EQ(vec(1), 99);
    EXPECT_EQ(vec(2), 99);
}

TEST(OperationIfTest, ScalarCondVectorValues)
{
    Value c1 = Value::Integer(1);
    VecXd vv1(3); vv1 << 1.0, 2.0, 3.0;
    Value v1 = Value::Vector(vv1);
    VecXd vv2(3); vv2 << 4.0, 5.0, 6.0;
    Value v2 = Value::Vector(vv2);
    Value el = Value::Real(99.0);
    Value result = OperationIf({c1, v1, c1, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 1.0);
    EXPECT_DOUBLE_EQ(vec(1), 2.0);
    EXPECT_DOUBLE_EQ(vec(2), 3.0);
}

TEST(OperationIfTest, MatrixElementWise)
{
    MatXd cm1(2, 2); cm1 << 1.0, 0.0, 0.0, 1.0;
    MatXd cm2(2, 2); cm2 << 0.0, 1.0, 1.0, 0.0;
    Value c1 = Value::Matrix(cm1);
    Value v1 = Value::Integer(10);
    Value c2 = Value::Matrix(cm2);
    Value v2 = Value::Integer(20);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto mat = result.as_measurement().as_matrix<int>();
    EXPECT_EQ(mat(0, 0), 10);  // c1 true
    EXPECT_EQ(mat(0, 1), 20);  // c2 true
    EXPECT_EQ(mat(1, 0), 20);  // c2 true
    EXPECT_EQ(mat(1, 1), 10);  // c1 true
}

TEST(OperationIfTest, TypePromotion)
{
    Value c1 = Value::Integer(1);
    Value v1 = Value::Integer(1);          // int
    Value c2 = Value::Integer(0);
    Value v2 = Value::Real(1.5);           // real
    Value el = Value::Real(2.5);           // real
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 1.0);
}

TEST(OperationIfTest, ComplexOperands)
{
    using namespace std::complex_literals;
    Value c1 = Value::Integer(1);
    Value v1 = Value::Complex(std::complex<double>(1.0, 2.0));
    Value c2 = Value::Integer(0);
    Value v2 = Value::Complex(std::complex<double>(3.0, 4.0));
    Value el = Value::Complex(std::complex<double>(5.0, 6.0));
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto z = result.as_measurement().as_scalar<std::complex<double>>();
    EXPECT_DOUBLE_EQ(z.real(), 1.0);
    EXPECT_DOUBLE_EQ(z.imag(), 2.0);
}

TEST(OperationIfTest, BoolCondition)
{
    Value c1 = Value::Boolean(true);
    Value v1 = Value::Integer(42);
    Value c2 = Value::Boolean(false);
    Value v2 = Value::Integer(99);
    Value el = Value::Integer(0);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 42);
}

TEST(OperationIfTest, NegativeConditionTrue)
{
    // Non-zero is truthy, including negative
    Value c1 = Value::Integer(-5);
    Value v1 = Value::Integer(42);
    Value el = Value::Integer(99);
    Value result = OperationIf({c1, v1, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<int>(), 42);
}

TEST(OperationIfTest, UnitPropagation)
{
    Unit u = Unit::parse("V");
    Value c1 = Value::Integer(1);
    Value v1 = Value::Real(10.0, u);
    Value c2 = Value::Integer(0);
    Value v2 = Value::Real(20.0, u);
    Value el = Value::Real(99.0, u);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_DOUBLE_EQ(result.as_measurement().as_scalar<double>(), 10.0);
    EXPECT_TRUE(result.as_measurement().unit().same_dimension(u));
}

TEST(OperationIfTest, UnitMismatchThrows)
{
    Unit uv = Unit::parse("V");
    Unit ua = Unit::parse("A");
    Value c1 = Value::Integer(1);
    Value v1 = Value::Real(10.0, uv);
    Value c2 = Value::Integer(0);
    Value v2 = Value::Real(20.0, ua);
    Value el = Value::Real(99.0, uv);
    EXPECT_THROW(OperationIf({c1, v1, c2, v2, el}), std::runtime_error);
}

TEST(OperationIfTest, EvenArityThrows)
{
    Value a = Value::Integer(1);
    Value b = Value::Integer(2);
    // 2 operands is even, should throw
    EXPECT_THROW(OperationIf({a, b}), std::runtime_error);
}

TEST(OperationIfTest, SingleOperandThrows)
{
    Value a = Value::Integer(1);
    EXPECT_THROW(OperationIf({a}), std::runtime_error);
}

TEST(OperationIfTest, EmptyThrows)
{
    EXPECT_THROW(OperationIf({}), std::runtime_error);
}

// --- String path ---

TEST(OperationIfTest, StringScalarFirstBranch)
{
    Value c1 = Value::Integer(1);
    Value v1 = Value::String("first");
    Value c2 = Value::Integer(0);
    Value v2 = Value::String("second");
    Value el = Value::String("else");
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<std::string>(), "first");
}

TEST(OperationIfTest, StringScalarSecondBranch)
{
    Value c1 = Value::Integer(0);
    Value v1 = Value::String("first");
    Value c2 = Value::Integer(1);
    Value v2 = Value::String("second");
    Value el = Value::String("else");
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<std::string>(), "second");
}

TEST(OperationIfTest, StringScalarElseBranch)
{
    Value c1 = Value::Integer(0);
    Value v1 = Value::String("first");
    Value c2 = Value::Integer(0);
    Value v2 = Value::String("second");
    Value el = Value::String("else");
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    EXPECT_EQ(result.as_measurement().as_scalar<std::string>(), "else");
}

TEST(OperationIfTest, StringVectorElementWise)
{
    VecXd cv1(3); cv1 << 1.0, 0.0, 0.0;
    VecXd cv2(3); cv2 << 0.0, 1.0, 0.0;
    Value c1 = Value::Vector(cv1);
    VecXs sv1(3); sv1(0) = "a"; sv1(1) = "b"; sv1(2) = "c";
    Value v1 = Value::Vector(sv1);
    Value c2 = Value::Vector(cv2);
    VecXs sv2(3); sv2(0) = "d"; sv2(1) = "e"; sv2(2) = "f";
    Value v2 = Value::Vector(sv2);
    VecXs se(3); se(0) = "x"; se(1) = "y"; se(2) = "z";
    Value el = Value::Vector(se);
    Value result = OperationIf({c1, v1, c2, v2, el});
    ASSERT_TRUE(result.is_measurement());
    auto vec = result.as_measurement().as_vector<std::string>();
    EXPECT_EQ(vec(0), "a");  // c1 true
    EXPECT_EQ(vec(1), "e");  // c2 true
    EXPECT_EQ(vec(2), "z");  // else
}

TEST(OperationIfTest, StringMixedNumericThrows)
{
    Value c1 = Value::Integer(1);
    Value v1 = Value::String("s");
    Value c2 = Value::Integer(0);
    Value v2 = Value::Integer(42);  // numeric!
    Value el = Value::String("else");
    EXPECT_THROW(OperationIf({c1, v1, c2, v2, el}), std::runtime_error);
}

// --- DataArray path ---

TEST(OperationIfTest, DataArrayElseTaken)
{
    auto ds = xdataset::DataSeries::CreateScalarFromVector<int>({99});
    Value arr_val(xdataset::DataArray::CreateIndependent(std::move(ds)));

    Value c1 = Value::Integer(0);
    Value v1 = Value::Integer(42);

    Value result = OperationIf({c1, v1, arr_val});
    ASSERT_FALSE(result.is_measurement());
    auto& data = result.as_data_array().data();
    EXPECT_EQ(data.size(), 1u);
    EXPECT_EQ(data.scalar_at<int>(0), 99);
}
