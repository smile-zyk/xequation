#include "data_series.h"
#include "data_frame.h"

#include <gtest/gtest.h>

#include <complex>
#include <string>
#include <vector>


using xdataset::DataKind;
using xdataset::DataSeries;
using xdataset::Measurement;
using xdataset::DataType;
using xdataset::Index;
using xdataset::MultiIndexSelector;
using xdataset::Unit;
using xdataset::VecXd;
using xdataset::VecXi;
using xdataset::VecXcd;
using xdataset::VecXs;
using xdataset::MatXd;
using xdataset::MatXi;
using xdataset::MatXcd;
using xdataset::VecXcd;
using xdataset::VecXs;
using xdataset::MatXd;
using xdataset::MatXi;
using xdataset::MatXcd;
using xdataset::MatXs;

// ---------------------------------------------------------------------------

// =========================================================================
//  Measurement -- construction, type queries, unit
// =========================================================================

TEST(CellTest, ScalarCellCreateAndMutate) {
    Measurement m = Measurement::Real(42.0);
    EXPECT_EQ(m.data_kind(), DataKind::kScalar);
    EXPECT_EQ(m.data_type(), DataType::kReal);
    EXPECT_DOUBLE_EQ(m.as_scalar<double>(), 42.0);

    m = Measurement::Real(3.5);
    EXPECT_DOUBLE_EQ(m.as_scalar<double>(), 3.5);
}

TEST(CellTest, IntegerCellDtype) {
    Measurement m = Measurement::Integer(7);
    EXPECT_EQ(m.data_type(), DataType::kInteger);
    EXPECT_EQ(m.as_scalar<int>(), 7);
}

TEST(CellTest, ComplexCellDtype) {
    using cd = std::complex<double>;
    Measurement m = Measurement::Complex(cd(1.0, 2.0));
    EXPECT_EQ(m.data_type(), DataType::kComplex);
    EXPECT_DOUBLE_EQ(m.as_scalar<cd>().real(), 1.0);
    EXPECT_DOUBLE_EQ(m.as_scalar<cd>().imag(), 2.0);
}

TEST(CellTest, StringCellDtype) {
    Measurement m = Measurement::String(std::string("hello"));
    EXPECT_EQ(m.data_type(), DataType::kString);
    EXPECT_EQ(m.as_scalar<std::string>(), "hello");
}

TEST(CellTest, AppendCellToSeries) {
    Measurement m = Measurement::Real(3.5);
    DataSeries s = DataSeries::CreateScalar<double>(0);
    s.append(Measurement::Real(1.25));
    s.append(m);
    ASSERT_EQ(s.size(), 2u);
    EXPECT_DOUBLE_EQ(s.scalar_at<double>(1), 3.5);
}

TEST(CellTest, AppendTypePromotionIntToReal) {
    Measurement int_cell = Measurement::Integer(10);
    DataSeries s = DataSeries::CreateScalar<double>(0);
    EXPECT_THROW(s.append(int_cell), std::bad_cast);
}

TEST(CellTest, AppendTypePromotionIntToComplex) {
    Measurement int_cell = Measurement::Integer(10);
    DataSeries s = DataSeries::CreateScalar<std::complex<double>>(0);
    EXPECT_THROW(s.append(int_cell), std::bad_cast);
}

TEST(CellTest, AppendTypePromotionRealToComplex) {
    Measurement real_cell = Measurement::Real(3.5);
    DataSeries s = DataSeries::CreateScalar<std::complex<double>>(0);
    EXPECT_THROW(s.append(real_cell), std::bad_cast);
}

TEST(CellTest, AppendStillThrowsOnCompleteMismatch) {
    // Vector DataSeries, trying to append a different-shaped vector - still throws.
    VecXd v(4); v << 1., 2., 3., 4.;
    DataSeries s = DataSeries::CreateVector<double>(3, 0);
    EXPECT_THROW(s.append(Measurement::Vector(v)), std::bad_cast);
}

TEST(CellTest, AppendVectorShapeMismatchThrows) {
    VecXd vd(3); vd << 1., 2., 3.;
    DataSeries s(DataType::kComplex, xdataset::DataShape::Vector(2));
    EXPECT_THROW(s.append(Measurement::Vector(vd)), std::bad_cast);
}

TEST(CellTest, AppendUnitMismatchThrows) {
    // First append succeeds and sets the series unit.
    Measurement m_m = Measurement::Real(1.0).set_unit(xdataset::Unit::parse("meter"));
    DataSeries s = DataSeries::CreateScalar<double>(0);
    s.append(m_m);
    EXPECT_TRUE(s.unit().same_dimension(xdataset::Unit::parse("meter")));

    // Subsequent append with incompatible unit must throw.
    Measurement m_s = Measurement::Real(2.0).set_unit(xdataset::Unit::parse("sec"));
    EXPECT_THROW(s.append(m_s), std::invalid_argument);
}

TEST(CellTest, CellAtRoundtripScalar) {
    DataSeries s = DataSeries::CreateScalarFromVector<int>(std::vector<int>{1, 2, 3});
    Measurement m = s.measurement_at(1);
    EXPECT_EQ(m.data_kind(), DataKind::kScalar);
    EXPECT_EQ(m.data_type(), DataType::kInteger);
    EXPECT_EQ(m.as_scalar<int>(), 2);
}

TEST(CellTest, CellAtRoundtripVector) {
    DataSeries vecs(DataType::kReal, xdataset::DataShape::Vector(3));
    vecs.resize(2);
    vecs.vector_at<double>(0) << 1.0, 2.0, 3.0;
    vecs.vector_at<double>(1) << 4.0, 5.0, 6.0;
    Measurement m = vecs.measurement_at(1);
    EXPECT_EQ(m.data_kind(), DataKind::kVector);
    EXPECT_DOUBLE_EQ(m.as_vector<double>()(0), 4.0);
    EXPECT_DOUBLE_EQ(m.as_vector<double>()(2), 6.0);
}

TEST(CellTest, CellAtRoundtripMatrix) {
    DataSeries mats(DataType::kReal, xdataset::DataShape::Matrix(2, 2));
    mats.resize(1);
    mats.matrix_at<double>(0) << 1.0, 2.0, 3.0, 4.0;
    Measurement m = mats.measurement_at(0);
    EXPECT_EQ(m.data_kind(), DataKind::kMatrix);
    EXPECT_DOUBLE_EQ(m.as_matrix<double>()(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(m.as_matrix<double>()(1, 1), 4.0);
}

// ---------------------------------------------------------------------------
// Iterator  forward traversal
// ---------------------------------------------------------------------------

// Unit
// ---------------------------------------------------------------------------

TEST(CellUnitTest, DefaultCellIsDimensionless)
{
    Measurement m;
    EXPECT_TRUE(m.unit().same_dimension(xdataset::Unit()));
}

TEST(CellUnitTest, CopyPropagatesUnit)
{
    Measurement m = Measurement::Real(3.14);
    m.set_unit(xdataset::Unit::parse("meter"));
    Measurement m2(m);
    EXPECT_TRUE(m2.unit().same_dimension(m.unit()));
}

TEST(CellUnitTest, MovePropagatesUnit)
{
    Measurement m = Measurement::Real(2.72);
    m.set_unit(xdataset::Unit::parse("Hz"));
    Measurement m2(std::move(m));
    EXPECT_TRUE(m2.unit().same_dimension(xdataset::Unit::parse("Hz")));
}

TEST(CellUnitTest, AssignPropagatesUnit)
{
    Measurement m1 = Measurement::Real(1.0);
    m1.set_unit(xdataset::Unit::parse("meter"));
    Measurement m2;
    m2 = m1;
    EXPECT_TRUE(m2.unit().same_dimension(xdataset::Unit::parse("meter")));
}

// ---------------------------------------------------------------------------
// Unit  DataSeries set_unit / canonicalize / canonicalized

// =========================================================================
//  Measurement: canonicalized
// =========================================================================

TEST(MeasurementCanonTest, CanonicalizedCmToMeter)
{
    Measurement m = Measurement::Real(5.0).set_unit(xdataset::Unit::parse("cm"));
    Measurement c = m.canonicalized();
    // 5 cm -> 0.05 m
    EXPECT_DOUBLE_EQ(c.as_scalar<double>(), 0.05);
    EXPECT_DOUBLE_EQ(c.unit().multiplier(), 1.0);
    EXPECT_TRUE(c.unit().same_dimension(xdataset::Unit::parse("meter")));
}

TEST(MeasurementCanonTest, CanonicalizedFastPath)
{
    Measurement m = Measurement::Real(3.0).set_unit(xdataset::Unit::parse("Hz"));  // already coherent SI
    Measurement c = m.canonicalized();
    EXPECT_DOUBLE_EQ(c.as_scalar<double>(), 3.0);
    EXPECT_TRUE(c.unit().same_dimension(xdataset::Unit::parse("Hz")));
}

TEST(MeasurementCanonTest, CanonicalizedStringNoValueChange)
{
    Measurement m = Measurement::String(std::string("hello")).set_unit(xdataset::Unit::parse("meter"));
    Measurement c = m.canonicalized();
    EXPECT_EQ(c.as_scalar<std::string>(), "hello");
    EXPECT_TRUE(c.unit().same_dimension(xdataset::Unit::parse("meter")));
}

// =========================================================================
//  MeasurementFormatter auto-scale
// =========================================================================

TEST(MeasurementFormatTest, AutoScaleMega)
{
    Measurement m = Measurement::Real(1e9).set_unit(xdataset::Unit::parse("Hz"));
    std::string s = m.to_string();
    // 1e9 Hz -> 1 GHz
    EXPECT_TRUE(s.find("GHz") != std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleMilli)
{
    Measurement m = Measurement::Real(0.002).set_unit(xdataset::Unit::parse("V"));
    std::string s = m.to_string();
    // 0.002 V -> 2 mV
    EXPECT_TRUE(s.find("2") != std::string::npos);
    EXPECT_TRUE(s.find("mV") != std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleKiloMeter)
{
    Measurement m = Measurement::Real(5000).set_unit(xdataset::Unit::parse("meter"));
    std::string s = m.to_string();
    // 5000 meter -> 5 Kmeter
    EXPECT_TRUE(s.find("5") != std::string::npos);
    EXPECT_TRUE(s.find("Kmeter") != std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleMilliMeter)
{
    Measurement m = Measurement::Real(0.003).set_unit(xdataset::Unit::parse("meter"));
    std::string s = m.to_string();
    // 0.003 meter -> 3 mmeter
    EXPECT_TRUE(s.find("3") != std::string::npos);
    EXPECT_TRUE(s.find("mmeter") != std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleNoneForDimensionless)
{
    Measurement m = Measurement::Real(3.14);
    std::string s = m.to_string();
    // 3.14 stays 3.14 (in [1, 1000))
    EXPECT_NE(s.find("3.14"), std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleMegaDimensionless)
{
    Measurement m = Measurement::Integer(1000000);
    std::string s = m.to_string();
    // 1000000 -> 1 M (dimensionless auto-scale)
    EXPECT_TRUE(s.find("M") != std::string::npos);
    EXPECT_TRUE(s.find("1") != std::string::npos);
}

TEST(MeasurementFormatTest, AutoScaleKiloFor100Hz)
{
    Measurement m = Measurement::Real(100.0).set_unit(xdataset::Unit::parse("Hz"));
    std::string s = m.to_string();
    // 100 Hz stays 100 Hz (1 <= 100 < 1000)
    EXPECT_TRUE(s.find("100") != std::string::npos);
}

// =========================================================================
//  Measurement: to_dataframe
// =========================================================================

TEST(MeasurementToDataFrameTest, Scalar)
{
    Measurement m = Measurement::Real(3.14).set_unit(xdataset::Unit::parse("meter"));
    auto df = m.to_dataframe("distance");

    EXPECT_EQ(df->row_count(), 1u);
    ASSERT_EQ(df->headers().size(), 1u);
    EXPECT_EQ(df->headers()[0], "distance");
    EXPECT_EQ(df->GetRow(0).fields[0].to_string(), "3.14 meter");
}

TEST(MeasurementToDataFrameTest, Vector)
{
    VecXd v(3);
    v << 1.0, 2.0, 3.0;
    Measurement m = Measurement::Vector(v);

    auto df = m.to_dataframe("pos");

    EXPECT_EQ(df->row_count(), 1u);
    ASSERT_EQ(df->headers().size(), 3u);
    EXPECT_EQ(df->headers()[0], "pos(1)");
    EXPECT_EQ(df->headers()[1], "pos(2)");
    EXPECT_EQ(df->headers()[2], "pos(3)");

    EXPECT_EQ(df->GetRow(0).fields[0].to_string(), "1");
    EXPECT_EQ(df->GetRow(0).fields[1].to_string(), "2");
    EXPECT_EQ(df->GetRow(0).fields[2].to_string(), "3");
}

TEST(MeasurementToDataFrameTest, Matrix)
{
    xdataset::MatXd mat(2, 2);
    mat << 1.0, 2.0, 3.0, 4.0;
    Measurement m = Measurement::Matrix(mat);

    auto df = m.to_dataframe("mat");

    EXPECT_EQ(df->row_count(), 1u);
    ASSERT_EQ(df->headers().size(), 4u);
    EXPECT_EQ(df->headers()[0], "mat(1,1)");
    EXPECT_EQ(df->headers()[1], "mat(1,2)");
    EXPECT_EQ(df->headers()[2], "mat(2,1)");
    EXPECT_EQ(df->headers()[3], "mat(2,2)");

    EXPECT_EQ(df->GetRow(0).fields[0].to_string(), "1");
    EXPECT_EQ(df->GetRow(0).fields[1].to_string(), "2");
    EXPECT_EQ(df->GetRow(0).fields[2].to_string(), "3");
    EXPECT_EQ(df->GetRow(0).fields[3].to_string(), "4");
}

TEST(MeasurementToDataFrameTest, ToCsvRoundtrip)
{
    VecXd v(2);
    v << 10.0, 20.0;
    Measurement m = Measurement::Vector(v);

    auto df = m.to_dataframe("velocity");
    const std::string csv = df->ToCsv();

    // Header row
    EXPECT_NE(csv.find(",velocity(1),velocity(2)"), std::string::npos);
    // Data row -> single measurement, no multi-index
    //   FormatMultiIndex() gives "[]", EscapeCsvField does not quote it
    //   (no comma / quote / newline characters), so: [],10,20
    EXPECT_NE(csv.find("0,10,20"), std::string::npos);
}

// =========================================================================
//  Measurement::at
// =========================================================================

TEST(MeasurementAtTest, ScalarThrows)
{
    Measurement m = Measurement::Real(42.0);
    EXPECT_THROW(m.at({MultiIndexSelector::Any()}), std::logic_error);
}

TEST(MeasurementAtTest, VectorAtEqualReturnsScalar)
{
    VecXd v(4); v << 10.0, 20.0, 30.0, 40.0;
    Measurement m = Measurement::Vector(v);

    Measurement result = m.at({MultiIndexSelector::Equal(2)});
    ASSERT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_DOUBLE_EQ(result.as_scalar<double>(), 30.0);
}

TEST(MeasurementAtTest, VectorAtInReturnsSubVector)
{
    VecXd v(5); v << 1.0, 2.0, 3.0, 4.0, 5.0;
    Measurement m = Measurement::Vector(v);

    Measurement result = m.at({MultiIndexSelector::In({0, 2, 4})});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec(0), 1.0);
    EXPECT_DOUBLE_EQ(vec(1), 3.0);
    EXPECT_DOUBLE_EQ(vec(2), 5.0);
}

TEST(MeasurementAtTest, VectorAtInPreservesUnit)
{
    Unit u = Unit::parse("V");
    VecXd v(3); v << 1.0, 2.0, 3.0;
    Measurement m = Measurement::Vector(v).set_unit(u);

    Measurement result = m.at({MultiIndexSelector::In({1, 2})});
    EXPECT_TRUE(result.unit().same_dimension(u));
}

TEST(MeasurementAtTest, VectorAtAnyReturnsAll)
{
    VecXd v(3); v << 1.0, 2.0, 3.0;
    Measurement m = Measurement::Vector(v);

    Measurement result = m.at({MultiIndexSelector::Any()});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
}

TEST(MeasurementAtTest, MatrixAtSingleElementReturnsScalar)
{
    MatXd mat(2, 3);
    mat << 1.0, 2.0, 3.0,
           4.0, 5.0, 6.0;
    Measurement m = Measurement::Matrix(mat);

    Measurement result = m.at({MultiIndexSelector::Equal(1), MultiIndexSelector::Equal(2)});
    ASSERT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_DOUBLE_EQ(result.as_scalar<double>(), 6.0);
}

TEST(MeasurementAtTest, MatrixAtSingleRowReturnsVector)
{
    MatXd mat(3, 3);
    mat << 1.0, 2.0, 3.0,
           4.0, 5.0, 6.0,
           7.0, 8.0, 9.0;
    Measurement m = Measurement::Matrix(mat);

    Measurement result = m.at({MultiIndexSelector::Equal(1), MultiIndexSelector::Any()});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec(0), 4.0);
    EXPECT_DOUBLE_EQ(vec(1), 5.0);
    EXPECT_DOUBLE_EQ(vec(2), 6.0);
}

TEST(MeasurementAtTest, MatrixAtSingleColumnReturnsVector)
{
    MatXd mat(3, 2);
    mat << 1.0, 2.0,
           3.0, 4.0,
           5.0, 6.0;
    Measurement m = Measurement::Matrix(mat);

    Measurement result = m.at({MultiIndexSelector::Any(), MultiIndexSelector::Equal(0)});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec(0), 1.0);
    EXPECT_DOUBLE_EQ(vec(1), 3.0);
    EXPECT_DOUBLE_EQ(vec(2), 5.0);
}

TEST(MeasurementAtTest, MatrixAtInReturnsSubMatrix)
{
    MatXd mat(4, 4);
    mat << 1.0,  2.0,  3.0,  4.0,
           5.0,  6.0,  7.0,  8.0,
           9.0,  10.0, 11.0, 12.0,
           13.0, 14.0, 15.0, 16.0;
    Measurement m = Measurement::Matrix(mat);

    Measurement result = m.at({MultiIndexSelector::In({0, 2}), MultiIndexSelector::In({1, 3})});
    ASSERT_EQ(result.data_kind(), DataKind::kMatrix);
    auto sub = result.as_matrix<double>();
    EXPECT_EQ(sub.rows(), 2);
    EXPECT_EQ(sub.cols(), 2);
    EXPECT_DOUBLE_EQ(sub(0, 0), 2.0);
    EXPECT_DOUBLE_EQ(sub(0, 1), 4.0);
    EXPECT_DOUBLE_EQ(sub(1, 0), 10.0);
    EXPECT_DOUBLE_EQ(sub(1, 1), 12.0);
}

TEST(MeasurementAtTest, MatrixAtPreservesUnit)
{
    Unit u = Unit::parse("V");
    MatXd mat(2, 2);
    mat << 1.0, 2.0, 3.0, 4.0;
    Measurement m = Measurement::Matrix(mat).set_unit(u);

    Measurement result = m.at({MultiIndexSelector::Equal(0), MultiIndexSelector::Equal(0)});
    ASSERT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_DOUBLE_EQ(result.as_scalar<double>(), 1.0);
    EXPECT_TRUE(result.unit().same_dimension(u));
}

TEST(MeasurementAtTest, IntegerVector)
{
    VecXi v(3); v << 10, 20, 30;
    Measurement m = Measurement::Vector(v);

    Measurement result = m.at({MultiIndexSelector::In({0, 2})});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<int>();
    EXPECT_EQ(vec(0), 10);
    EXPECT_EQ(vec(1), 30);
}

TEST(MeasurementAtTest, ComplexMatrix)
{
    MatXcd mat(2, 2);
    mat << std::complex<double>(1, 0), std::complex<double>(2, 0),
           std::complex<double>(3, 0), std::complex<double>(4, 0);
    Measurement m = Measurement::Matrix(mat);

    Measurement result = m.at({MultiIndexSelector::Any(), MultiIndexSelector::Equal(1)});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<std::complex<double>>();
    EXPECT_DOUBLE_EQ(vec(0).real(), 2.0);
    EXPECT_DOUBLE_EQ(vec(1).real(), 4.0);
}

TEST(MeasurementAtTest, StringVector)
{
    VecXs v(3);
    v(0) = "a"; v(1) = "b"; v(2) = "c";
    Measurement m = Measurement::Vector(v);

    Measurement result = m.at({MultiIndexSelector::In({1, 2})});
    ASSERT_EQ(result.data_kind(), DataKind::kVector);
    auto vec = result.as_vector<std::string>();
    EXPECT_EQ(vec(0), "b");
    EXPECT_EQ(vec(1), "c");
}

TEST(MeasurementAtTest, TooManySelectorsThrows)
{
    VecXd v(3); v << 1.0, 2.0, 3.0;
    Measurement m = Measurement::Vector(v);
    EXPECT_THROW(m.at({MultiIndexSelector::Any(), MultiIndexSelector::Any()}), std::invalid_argument);
}

// =========================================================================
//  Measurement::transform
// =========================================================================

TEST(MeasurementTransformTest, ScalarSquare) {
    Measurement m = Measurement::Real(3.0);
    Measurement result = m.transform([](double x) { return x * x; });
    EXPECT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_EQ(result.data_type(), DataType::kReal);
    EXPECT_DOUBLE_EQ(result.as_scalar<double>(), 9.0);
}

TEST(MeasurementTransformTest, ScalarIntNegate) {
    Measurement m = Measurement::Integer(5);
    Measurement result = m.transform([](int x) { return -x; });
    EXPECT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_EQ(result.data_type(), DataType::kInteger);
    EXPECT_EQ(result.as_scalar<int>(), -5);
}

TEST(MeasurementTransformTest, ScalarComplexAbsToReal) {
    Measurement m = Measurement::Complex(std::complex<double>(3.0, 4.0));
    Measurement result = m.transform([](std::complex<double> x) { return std::abs(x); });
    EXPECT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_EQ(result.data_type(), DataType::kReal);
    EXPECT_DOUBLE_EQ(result.as_scalar<double>(), 5.0);
}

TEST(MeasurementTransformTest, ScalarDoubleToInt) {
    Measurement m = Measurement::Real(2.7);
    Measurement result = m.transform([](double x) { return static_cast<int>(x); });
    EXPECT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_EQ(result.data_type(), DataType::kInteger);
    EXPECT_EQ(result.as_scalar<int>(), 2);
}

TEST(MeasurementTransformTest, ScalarString) {
    Measurement m = Measurement::String("hello");
    Measurement result = m.transform([](const std::string& s) { return s + "!"; });
    EXPECT_EQ(result.data_kind(), DataKind::kScalar);
    EXPECT_EQ(result.data_type(), DataType::kString);
    EXPECT_EQ(result.as_scalar<std::string>(), "hello!");
}

TEST(MeasurementTransformTest, VectorSquare) {
    VecXd v(3); v << 1.0, 2.0, 3.0;
    Measurement m = Measurement::Vector(v);
    Measurement result = m.transform([](double x) { return x * x; });
    EXPECT_EQ(result.data_kind(), DataKind::kVector);
    EXPECT_EQ(result.data_type(), DataType::kReal);
    auto vec = result.as_vector<double>();
    EXPECT_DOUBLE_EQ(vec(0), 1.0);
    EXPECT_DOUBLE_EQ(vec(1), 4.0);
    EXPECT_DOUBLE_EQ(vec(2), 9.0);
}

TEST(MeasurementTransformTest, VectorIntNegate) {
    VecXi v(3); v << 1, -2, 3;
    Measurement m = Measurement::Vector(v);
    Measurement result = m.transform([](int x) { return -x; });
    EXPECT_EQ(result.data_kind(), DataKind::kVector);
    EXPECT_EQ(result.data_type(), DataType::kInteger);
    auto vec = result.as_vector<int>();
    EXPECT_EQ(vec(0), -1);
    EXPECT_EQ(vec(1), 2);
    EXPECT_EQ(vec(2), -3);
}

TEST(MeasurementTransformTest, MatrixSquare) {
    MatXd m(2, 2);
    m << 1.0, 2.0, 3.0, 4.0;
    Measurement meas = Measurement::Matrix(m);
    Measurement result = meas.transform([](double x) { return x * x; });
    EXPECT_EQ(result.data_kind(), DataKind::kMatrix);
    EXPECT_EQ(result.data_type(), DataType::kReal);
    auto mat = result.as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(mat(0, 1), 4.0);
    EXPECT_DOUBLE_EQ(mat(1, 0), 9.0);
    EXPECT_DOUBLE_EQ(mat(1, 1), 16.0);
}

TEST(MeasurementTransformTest, MatrixComplexAbsToReal) {
    MatXcd m(2, 2);
    m << std::complex<double>(3.0, 4.0), std::complex<double>(0.0, -1.0),
         std::complex<double>(-5.0, 0.0), std::complex<double>(1.0, 1.0);
    Measurement meas = Measurement::Matrix(m);
    Measurement result = meas.transform([](std::complex<double> x) { return std::abs(x); });
    EXPECT_EQ(result.data_kind(), DataKind::kMatrix);
    EXPECT_EQ(result.data_type(), DataType::kReal);
    auto mat = result.as_matrix<double>();
    EXPECT_DOUBLE_EQ(mat(0, 0), 5.0);
    EXPECT_DOUBLE_EQ(mat(0, 1), 1.0);
    EXPECT_DOUBLE_EQ(mat(1, 0), 5.0);
    EXPECT_NEAR(mat(1, 1), std::sqrt(2.0), 1e-12);
}

// =========================================================================
