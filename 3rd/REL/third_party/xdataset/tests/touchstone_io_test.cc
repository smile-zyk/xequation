#include "data_array_io.h"
#include "dataset_io.h"
#include "dataset.h"
#include "block.h"
#include "block_fixtures.h"

#include <gtest/gtest.h>

#include <complex>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace xdataset
{
    namespace
    {
        using namespace block_fixtures;

        // --------------------------------------------------------------------
        // Helpers: create test touchstone files on disk
        // --------------------------------------------------------------------

        void write_test_s2p_ri(const std::string& path)
        {
            std::ofstream out(path.c_str());
            out << "! Test Touchstone file\n";
            out << "# GHz S RI R 50\n";
            out << "! freq      reS11   imS11   reS21   imS21   reS12   imS12   reS22   imS22\n";
            out << "1.0   0.1  0.01   0.8  0.02   0.05  0.005  0.2  0.03\n";
            out << "2.0   0.15 0.015  0.75 0.025  0.06  0.006  0.25 0.035\n";
            out << "3.0   0.2  0.02   0.7  0.03   0.07  0.007  0.3  0.04\n";
            out.close();
        }

        void write_test_s2p_ma(const std::string& path)
        {
            std::ofstream out(path.c_str());
            out << "! Test Touchstone file (MA format)\n";
            out << "# GHz S MA R 50\n";
            out << "1.0   0.1  45.0   0.8  -30.0   0.05  90.0   0.2  -60.0\n";
            out << "2.0   0.15 50.0   0.75 -35.0   0.06  85.0   0.25 -55.0\n";
            out << "3.0   0.2  55.0   0.7  -40.0   0.07  80.0   0.3  -50.0\n";
            out.close();
        }

        void write_test_s1p_ri(const std::string& path)
        {
            std::ofstream out(path.c_str());
            out << "! Test Touchstone file (1-port)\n";
            out << "# GHz S RI R 50\n";
            out << "! freq      reS11   imS11\n";
            out << "1.0   0.1  0.01\n";
            out << "2.0   0.15 0.015\n";
            out << "3.0   0.2  0.02\n";
            out.close();
        }

        // --------------------------------------------------------------------
        // Helpers: build DataArrays suitable for Touchstone IO
        // --------------------------------------------------------------------

        DataArray make_s2p_data_array()
        {
            std::vector<double> freqs = {1.0, 2.0, 3.0};
            std::vector<std::complex<double>> s_flat = {
                {0.1, 0.01}, {0.05, 0.005}, {0.8, 0.02},  {0.2, 0.03},
                {0.15, 0.015}, {0.06, 0.006}, {0.75, 0.025}, {0.25, 0.035},
                {0.2, 0.02}, {0.07, 0.007}, {0.7, 0.03},   {0.3, 0.04},
            };

            DataArrayCreateInfo info;
            info.datas["freq"] = DataSeries::CreateScalarFromMemory<double>(
                freqs.data(), freqs.size());
            info.datas[DataArray::kSelf] = DataSeries::CreateMatrixFromMemory<std::complex<double>>(
                2, 2, s_flat.data(), s_flat.size());
            info.multi_dimension_spec.add_regular(3);
            info.kind = DataArrayKind::kDependent;

            return DataArray(info);
        }

        DataArray make_s1p_data_array()
        {
            std::vector<double> freqs = {1.0, 2.0, 3.0};
            std::vector<std::complex<double>> s_flat = {
                {0.1, 0.01}, {0.15, 0.015}, {0.2, 0.02}};

            DataArrayCreateInfo info;
            info.datas["freq"] = DataSeries::CreateScalarFromMemory<double>(
                freqs.data(), freqs.size());
            info.datas[DataArray::kSelf] = DataSeries::CreateMatrixFromMemory<std::complex<double>>(
                1, 1, s_flat.data(), s_flat.size());
            info.multi_dimension_spec.add_regular(3);
            info.kind = DataArrayKind::kDependent;

            return DataArray(info);
        }

    } // namespace

    // ========================================================================
    // DataArray-level Touchstone IO
    // ========================================================================

    TEST(TouchstoneDataArrayIoTest, ReadS2P_RiFormat)
    {
        write_test_s2p_ri("test_da_s2p_ri.s2p");
        DataArray da = DataArrayIO::Load("touchstone", "test_da_s2p_ri.s2p");
        std::remove("test_da_s2p_ri.s2p");

        EXPECT_EQ(da.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(da.multi_dimension_spec().rank(), 1u);

        const DataSeries& freq = da.datas().find("freq")->second;
        // GHz values canonicalized to Hz (x 1e9)
        EXPECT_DOUBLE_EQ(freq.scalar_at<double>(0), 1e9);
        EXPECT_DOUBLE_EQ(freq.scalar_at<double>(2), 3e9);

        // Unit from option line "# GHz ...", canonicalized to base Hz
        EXPECT_EQ(freq.unit().to_string(), "Hz");

        const DataSeries& s = da.data();
        EXPECT_EQ(s.data_kind(), DataKind::kMatrix);
        EXPECT_EQ(s.data_shape()[0], 2);
        EXPECT_EQ(s.data_shape()[1], 2);

        const auto& mat0 = s.matrix_at<std::complex<double>>(0);
        EXPECT_NEAR(mat0(0, 0).real(), 0.1, 1e-9);
        EXPECT_NEAR(mat0(0, 0).imag(), 0.01, 1e-9);

        const auto& mat2 = s.matrix_at<std::complex<double>>(2);
        EXPECT_NEAR(mat2(1, 1).real(), 0.3, 1e-9);
        EXPECT_NEAR(mat2(1, 1).imag(), 0.04, 1e-9);
    }

    TEST(TouchstoneDataArrayIoTest, ReadS2P_MaFormat)
    {
        write_test_s2p_ma("test_da_s2p_ma.s2p");
        DataArray da = DataArrayIO::Load("touchstone", "test_da_s2p_ma.s2p");
        std::remove("test_da_s2p_ma.s2p");

        const auto& mat0 = da.data().matrix_at<std::complex<double>>(0);
        double expected_real = 0.1 * std::cos(45.0 * M_PI / 180.0);
        double expected_imag = 0.1 * std::sin(45.0 * M_PI / 180.0);
        EXPECT_NEAR(mat0(0, 0).real(), expected_real, 1e-9);
        EXPECT_NEAR(mat0(0, 0).imag(), expected_imag, 1e-9);
    }

    TEST(TouchstoneDataArrayIoTest, ReadS1P_RiFormat)
    {
        write_test_s1p_ri("test_da_s1p.s1p");
        DataArray da = DataArrayIO::Load("touchstone", "test_da_s1p.s1p");
        std::remove("test_da_s1p.s1p");

        const auto& mat1 = da.data().matrix_at<std::complex<double>>(1);
        EXPECT_NEAR(mat1(0, 0).real(), 0.15, 1e-9);
        EXPECT_NEAR(mat1(0, 0).imag(), 0.015, 1e-9);
    }

    TEST(TouchstoneDataArrayIoTest, ReadS2P_AliasSnpFormat)
    {
        write_test_s2p_ri("test_da_alias.s2p");
        DataArray da = DataArrayIO::Load("snp", "test_da_alias.s2p");
        std::remove("test_da_alias.s2p");

        EXPECT_EQ(da.data_kind(), DataArrayKind::kDependent);
    }

    TEST(TouchstoneDataArrayIoTest, WriteAndReadS2P)
    {
        DataArray da = make_s2p_data_array();
        DataArrayIO::Save(da, "touchstone", "test_da_wr_s2p.s2p");
        DataArray loaded = DataArrayIO::Load("touchstone", "test_da_wr_s2p.s2p");
        std::remove("test_da_wr_s2p.s2p");

        const auto& mat0 = loaded.data().matrix_at<std::complex<double>>(0);
        const auto& mat1 = loaded.data().matrix_at<std::complex<double>>(1);
        EXPECT_NEAR(std::abs(mat0(0, 0)), std::abs(std::complex<double>(0.1, 0.01)), 1e-6);
        EXPECT_NEAR(std::abs(mat1(1, 0)), std::abs(std::complex<double>(0.75, 0.025)), 1e-6);
    }

    TEST(TouchstoneDataArrayIoTest, WriteAndReadS1P)
    {
        DataArray da = make_s1p_data_array();
        DataArrayIO::Save(da, "touchstone", "test_da_wr_s1p.s1p");
        DataArray loaded = DataArrayIO::Load("touchstone", "test_da_wr_s1p.s1p");
        std::remove("test_da_wr_s1p.s1p");

        EXPECT_EQ(loaded.data().data_shape()[0], 1);
        EXPECT_EQ(loaded.data().data_shape()[1], 1);
        EXPECT_EQ(loaded.data().size(), 3u);
    }

    TEST(TouchstoneDataArrayIoTest, WriteAliasSnp)
    {
        DataArray da = make_s2p_data_array();
        DataArrayIO::Save(da, "snp", "test_da_alias_w.s2p");
        DataArray loaded = DataArrayIO::Load("snp", "test_da_alias_w.s2p");
        std::remove("test_da_alias_w.s2p");

        EXPECT_EQ(loaded.data().data_shape()[0], 2);
    }

    TEST(TouchstoneDataArrayIoTest, WriterReaderDirect)
    {
        DataArray da = make_s2p_data_array();
        DataArrayIO::Save(da, "touchstone", "test_da_direct.s2p");
        DataArray loaded = DataArrayIO::Load("touchstone", "test_da_direct.s2p");
        std::remove("test_da_direct.s2p");

        EXPECT_EQ(loaded.data().data_shape()[0], 2);
        EXPECT_EQ(loaded.data().data_shape()[1], 2);
    }

    // ========================================================================
    // Dataset-level Touchstone reader (convenience)
    // ========================================================================

    TEST(TouchstoneDatasetReaderTest, ReadS2PIntoDataset)
    {
        write_test_s2p_ri("test_ds_s2p.s2p");
        Dataset ds = DatasetIO::Load("touchstone", "test_ds_s2p.s2p");
        std::remove("test_ds_s2p.s2p");

        EXPECT_EQ(ds.block_count(), 1u);
        EXPECT_TRUE(ds.IsLeaf("SP"));

        const Block& b = ds.GetBlock("SP");
        EXPECT_EQ(b.independents().size(), 1u);
        EXPECT_EQ(b.dependents().size(), 1u);

        const DependentSpec& s_dep = b.dependent_spec("S");
        const auto& mat0 = s_dep.data.matrix_at<std::complex<double>>(0);
        EXPECT_NEAR(mat0(0, 0).real(), 0.1, 1e-9);

        // Freq unit carried through to Dataset (canonicalized to Hz)
        const IndependentSpec& freq_is = b.independent_spec("freq");
        EXPECT_EQ(freq_is.data.unit().to_string(), "Hz");
        EXPECT_DOUBLE_EQ(freq_is.data.scalar_at<double>(0), 1e9);
    }

    TEST(TouchstoneDatasetReaderTest, ReadS1PIntoDataset)
    {
        write_test_s1p_ri("test_ds_s1p.s1p");
        Dataset ds = DatasetIO::Load("touchstone", "test_ds_s1p.s1p");
        std::remove("test_ds_s1p.s1p");

        EXPECT_TRUE(ds.IsLeaf("SP"));
        const Block& b = ds.GetBlock("SP");
        EXPECT_EQ(b.independents().size(), 1u);
        EXPECT_EQ(b.dependents().size(), 1u);
    }

    // ========================================================================
    // Error Handling
    // ========================================================================

    TEST(TouchstoneDataArrayIoTest, MissingFileThrows)
    {
        EXPECT_THROW({
            DataArrayIO::Load("touchstone", "nonexistent.s2p");
        }, std::runtime_error);
    }

    TEST(TouchstoneDataArrayIoTest, NoOptionLineThrows)
    {
        {
            std::ofstream out("test_bad.s2p");
            out << "! comment only, no option line\n";
            out << "1.0 0.1 0.01\n";
            out.close();
        }
        EXPECT_THROW({
            DataArrayIO::Load("touchstone", "test_bad.s2p");
        }, std::runtime_error);
        std::remove("test_bad.s2p");
    }

    TEST(TouchstoneDataArrayIoTest, UnsupportedFormatThrows)
    {
        DataArray da = make_s2p_data_array();
        EXPECT_THROW({
            DataArrayIO::Save(da, "json", "test.json");
        }, std::invalid_argument);
        EXPECT_THROW({
            DataArrayIO::Load("json", "test.json");
        }, std::invalid_argument);
    }

    // ========================================================================
    // Real-world Murata s2p file
    // ========================================================================

    TEST(TouchstoneDataArrayIoTest, ReadMurataInductorS2P)
    {
        DataArray da = DataArrayIO::Load("touchstone",
            "../tests/case/LQP01HV0N3B02.s2p");

        EXPECT_EQ(da.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(da.multi_dimension_spec().rank(), 1u);

        const DataSeries& s = da.data();
        EXPECT_EQ(s.data_kind(), DataKind::kMatrix);
        EXPECT_EQ(s.data_shape()[0], 2);
        EXPECT_EQ(s.data_shape()[1], 2);

        // 401 frequency points
        EXPECT_EQ(s.size(), 401u);

        // Verify first frequency point
        const DataSeries& freq = da.datas().find("freq")->second;
        EXPECT_NEAR(freq.scalar_at<double>(0), 5.0e7, 1.0);

        // Unit from option line "# Hz ..."
        EXPECT_EQ(freq.unit().to_string(), "Hz");

        // Verify first S11 value
        const auto& mat0 = s.matrix_at<std::complex<double>>(0);
        EXPECT_NEAR(mat0(0, 0).real(), 1.3145386801343174e-4, 1e-14);
        EXPECT_NEAR(mat0(0, 0).imag(), 7.817132525456352e-4, 1e-14);

        // Output DataFrame CSV to file
        const DataFrame& df = da.GetOrCreateDataFrame("S");
        df.WriteToCsv("LQP01HV0N3B02.csv");
        std::cout << "CSV written to LQP01HV0N3B02.csv (" << df.row_count() << " rows)\n";
    }

} // namespace xdataset
