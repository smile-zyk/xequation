#include "dataset_io.h"
#include "dataset.h"
#include "block.h"
#include "block_fixtures.h"

#include <gtest/gtest.h>

#include <complex>
#include <cmath>
#include <cstdio>
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
        // Helpers
        // --------------------------------------------------------------------

        BlockCreateInfo make_simple_info()
        {
            // R(2) x freq(3): 6 rows
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"R", MakeScalarSeries(2),
                                DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"freq", MakeScalarSeriesFrom({1.0, 2.0, 3.0}),
                                DimensionSpec::Regular(3)});
            info.dependent_specs.push_back(
                DependentSpec{"Vout", MakeScalarSeries(6)});
            return info;
        }

        BlockCreateInfo make_vector_info()
        {
            // Regular(2) �?2 rows, vector width 3
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeries(2),
                                DimensionSpec::Regular(2)});
            info.dependent_specs.push_back(
                DependentSpec{"v", MakeVectorSeries(2, 3)});
            return info;
        }

    } // namespace

    // ========================================================================
    // Save / Load
    // ========================================================================

    TEST(Hdf5IoTest, SaveAndLoadSimpleRoundtrip)
    {
        Dataset ds("test_ds");
        ds.AddBlock("group/blk", make_simple_info());

        // Save
        DatasetIO::Save(ds, "hdf5", "test_roundtrip.h5");

        // Load
        Dataset loaded = DatasetIO::Load("hdf5", "test_roundtrip.h5");

        EXPECT_EQ(loaded.name(), "test_ds");
        EXPECT_EQ(loaded.block_count(), 1u);
        EXPECT_TRUE(loaded.IsLeaf("group/blk"));

        const Block& b = loaded.GetBlock("group/blk");
        EXPECT_EQ(b.independents().size(), 2u);
        EXPECT_EQ(b.dependents().size(), 1u);

        // Verify independent data
        const IndependentSpec& r = b.independent_spec("R");
        EXPECT_EQ(r.data.size(), 2u);
        EXPECT_EQ(r.data.scalar_at<double>(0), 0.0);
        EXPECT_TRUE(r.dimension.is_regular());
        EXPECT_EQ(r.dimension.regular_size(), 2u);

        const IndependentSpec& freq = b.independent_spec("freq");
        EXPECT_EQ(freq.data.size(), 3u);
        EXPECT_DOUBLE_EQ(freq.data.scalar_at<double>(0), 1.0);
        EXPECT_DOUBLE_EQ(freq.data.scalar_at<double>(2), 3.0);
        EXPECT_TRUE(freq.dimension.is_regular());
        EXPECT_EQ(freq.dimension.regular_size(), 3u);

        // Verify dependent
        const DependentSpec& vout = b.dependent_spec("Vout");
        EXPECT_EQ(vout.data.size(), 6u);
    }

    TEST(Hdf5IoTest, SaveAndLoadNestedBlocks)
    {
        Dataset ds("nested");
        ds.AddBlock("simulation/SP1/SP", make_simple_info());
        ds.AddBlock("simulation/SP1/HB", make_simple_info());
        ds.AddBlock("summary/stats", make_vector_info());

        DatasetIO::Save(ds, "hdf5", "test_nested.h5");
        Dataset loaded = DatasetIO::Load("hdf5", "test_nested.h5");

        EXPECT_EQ(loaded.name(), "nested");
        EXPECT_EQ(loaded.block_count(), 3u);
        EXPECT_TRUE(loaded.IsLeaf("simulation/SP1/SP"));
        EXPECT_TRUE(loaded.IsLeaf("simulation/SP1/HB"));
        EXPECT_TRUE(loaded.IsLeaf("summary/stats"));

        // Group structure is preserved
        EXPECT_TRUE(loaded.Exists("simulation"));
        EXPECT_TRUE(loaded.Exists("simulation/SP1"));
    }

    TEST(Hdf5IoTest, SaveAndLoadVectorDependent)
    {
        Dataset ds("vec_ds");
        ds.AddBlock("results", make_vector_info());

        DatasetIO::Save(ds, "hdf5", "test_vector.h5");
        Dataset loaded = DatasetIO::Load("hdf5", "test_vector.h5");

        const Block& b = loaded.GetBlock("results");
        const DependentSpec& v = b.dependent_spec("v");
        EXPECT_EQ(v.data.data_kind(), DataKind::kVector);
        EXPECT_EQ(v.data.data_shape().size(), 1u);
        EXPECT_EQ(v.data.data_shape()[0], 3);
        EXPECT_EQ(v.data.size(), 2u);
    }

    TEST(Hdf5IoTest, SaveAndLoadWithRaggedDimension)
    {
        Dataset ds("ragged");
        ds.AddBlock("data", MakeRaggedCreateInfo());

        DatasetIO::Save(ds, "hdf5", "test_ragged.h5");
        Dataset loaded = DatasetIO::Load("hdf5", "test_ragged.h5");

        EXPECT_EQ(loaded.block_count(), 1u);
        const Block& b = loaded.GetBlock("data");

        // Check ragged dimension
        const IndependentSpec& y = b.independent_spec("y");
        EXPECT_TRUE(y.dimension.is_ragged());
        EXPECT_EQ(y.dimension.ragged_sizes().size(), 2u);
        EXPECT_EQ(y.dimension.ragged_sizes()[0], 1u);
        EXPECT_EQ(y.dimension.ragged_sizes()[1], 2u);

        // Also check x is regular
        const IndependentSpec& x = b.independent_spec("x");
        EXPECT_TRUE(x.dimension.is_regular());
        EXPECT_EQ(x.dimension.regular_size(), 2u);
    }

    TEST(Hdf5IoTest, SaveAndGetDataArray) {
        Dataset ds("da_ds");
        ds.AddBlock("b", make_simple_info());

        DatasetIO::Save(ds, "hdf5", "test_da.h5");
        Dataset loaded = DatasetIO::Load("hdf5", "test_da.h5");

        const DataArray& da = loaded.GetDataArray("b", "freq");
        EXPECT_EQ(da.data_kind(), DataArrayKind::kIndependent);

        const DataArray& dep = loaded.GetDataArray("b", "Vout");
        EXPECT_EQ(dep.data_kind(), DataArrayKind::kDependent);
    }

    TEST(Hdf5IoTest, WriterReaderDirect) {
        Dataset ds("direct");
        ds.AddBlock("a", make_simple_info());

        DatasetIO::Save(ds, "hdf5", "test_direct.h5");
        Dataset loaded = DatasetIO::Load("hdf5", "test_direct.h5");

        EXPECT_EQ(loaded.name(), "direct");
        EXPECT_EQ(loaded.block_count(), 1u);
        EXPECT_TRUE(loaded.IsLeaf("a"));
    }

    TEST(Hdf5IoTest, UnsupportedFormatThrows)
    {
        Dataset ds("test");
        EXPECT_THROW({ DatasetIO::Save(ds, "json", "test.json"); },
                     std::invalid_argument);
    }

    // ========================================================================
    // Realistic multi-dimensional LNA simulation Dataset
    // ========================================================================

    TEST(Hdf5IoTest, BuildAndSaveLnaSimulation)
    {
        // Simulate an LNA design across 4 analyses:
        //   amplifier/DC     -- DC bias sweep:  Vgs  x Vds  -> Id, gm, Vth
        //   amplifier/SP1    -- S-parameters:   freq         -> S (2x2 complex)
        //   amplifier/HB1    -- Harmonic bal:   Pin           -> Pout, Gain, PAE
        //   amplifier/noise  -- Noise summary:  freq          -> NFmin, Rn

        Dataset ds("LNA_Design");

        // =====================================================================
        // DC sweep: Vgs (0.5..1.5 V, 11 steps) x Vds (1.0..5.0 V, 9 steps)
        // =====================================================================
        {
            const std::size_t nVgs = 11, nVds = 9, total = nVgs * nVds;

            std::vector<double> vgs_raw(nVgs), vds_raw(nVds);
            for (std::size_t i = 0; i < nVgs; ++i) vgs_raw[i] = 0.50 + i * 0.10;
            for (std::size_t j = 0; j < nVds; ++j) vds_raw[j] = 1.00 + j * 0.50;

            DataSeries vgs_series = DataSeries::CreateScalarFromMemory<double>(vgs_raw.data(), nVgs);
            vgs_series.set_unit("V");
            DataSeries vds_series = DataSeries::CreateScalarFromMemory<double>(vds_raw.data(), nVds);
            vds_series.set_unit("V");

            const double kp = 0.12, vth0 = 0.55, lam = 0.04;
            std::vector<double> id(total), gm_val(total), vth_val(total);
            for (std::size_t j = 0; j < nVds; ++j)
                for (std::size_t i = 0; i < nVgs; ++i)
                {
                    std::size_t idx = j * nVgs + i;
                    double Veff = vgs_raw[i] - vth0;
                    if (Veff < 0.01) Veff = 0.01;
                    id[idx]      = kp * Veff * Veff * (1.0 + lam * vds_raw[j]);
                    gm_val[idx]  = 2.0 * kp * Veff * (1.0 + lam * vds_raw[j]);
                    vth_val[idx] = vth0 + 0.02 * vds_raw[j];
                }

            BlockCreateInfo info;
            info.independent_specs.push_back(IndependentSpec{"Vgs", std::move(vgs_series), DimensionSpec::Regular(nVgs)});
            info.independent_specs.push_back(IndependentSpec{"Vds", std::move(vds_series), DimensionSpec::Regular(nVds)});

            auto id_ds = DataSeries::CreateScalarFromMemory<double>(id.data(), total);
            id_ds.set_unit("A");
            auto gm_ds = DataSeries::CreateScalarFromMemory<double>(gm_val.data(), total);
            gm_ds.set_unit("S");
            auto vth_ds = DataSeries::CreateScalarFromMemory<double>(vth_val.data(), total);
            vth_ds.set_unit("V");

            info.dependent_specs.push_back(DependentSpec{"Id",  std::move(id_ds)});
            info.dependent_specs.push_back(DependentSpec{"gm",  std::move(gm_ds)});
            info.dependent_specs.push_back(DependentSpec{"Vth", std::move(vth_ds)});

            ds.AddBlock("amplifier/DC/bias", std::move(info));
        }

        // =====================================================================
        // S-parameters: freq 1..10 GHz, 10 steps, 2-port
        // =====================================================================
        {
            const std::size_t Nf = 10;
            std::vector<double> freqs(Nf);
            for (std::size_t i = 0; i < Nf; ++i) freqs[i] = 1.0 + i * 1.0;

            DataSeries freq_series = DataSeries::CreateScalarFromMemory<double>(freqs.data(), Nf);
            freq_series.set_unit("GHz");

            auto from_db_angle = [](double db, double deg) -> std::complex<double> {
                double mag = std::pow(10.0, db / 20.0);
                double rad = deg * M_PI / 180.0;
                return {mag * std::cos(rad), mag * std::sin(rad)};
            };

            std::vector<std::complex<double>> s_flat(Nf * 4);
            for (std::size_t i = 0; i < Nf; ++i)
            {
                double f = freqs[i];
                double s21_db = 15.0;
                if (f > 4.0) s21_db -= (f - 4.0) * 0.8;
                double s11_db = -12.0 + (f - 1.0) * 0.65;
                double s22_db = -10.0 + (f - 1.0) * 0.3;
                double s12_db = -26.0 + (f - 1.0) * 0.1;

                double s11_ang = -f * 45.0, s21_ang = 180.0 - f * 18.0;
                double s12_ang = 90.0 - f * 5.0,  s22_ang = -f * 30.0;

                std::size_t base = i * 4;
                s_flat[base + 0] = from_db_angle(s11_db, s11_ang);
                s_flat[base + 1] = from_db_angle(s12_db, s12_ang);
                s_flat[base + 2] = from_db_angle(s21_db, s21_ang);
                s_flat[base + 3] = from_db_angle(s22_db, s22_ang);
            }

            BlockCreateInfo info;
            info.independent_specs.push_back(IndependentSpec{"freq", std::move(freq_series), DimensionSpec::Regular(Nf)});
            info.dependent_specs.push_back(DependentSpec{"S", DataSeries::CreateMatrixFromMemory<std::complex<double>>(2, 2, s_flat.data(), s_flat.size())});
            ds.AddBlock("amplifier/SP1/SP", std::move(info));
        }

        // =====================================================================
        // Harmonic balance: Pin -20..+10 dBm, 7 steps
        // =====================================================================
        {
            const std::size_t Np = 7;
            std::vector<double> pin_vals(Np);
            for (std::size_t i = 0; i < Np; ++i) pin_vals[i] = -20.0 + i * 5.0;

            DataSeries pin_series = DataSeries::CreateScalarFromMemory<double>(pin_vals.data(), Np);
            pin_series.set_unit("W");

            std::vector<double> pout(Np), gain(Np), pae(Np);
            for (std::size_t i = 0; i < Np; ++i)
            {
                double pin = pin_vals[i];
                double comp = (pin > -10.0) ? 0.28 * (pin + 10.0) : 0.0;
                double g = 15.2 - comp;
                gain[i] = g;
                pout[i] = pin + g;
                pae[i] = 2.0 + (pin + 20.0) * 0.90;
                if (pae[i] > 22.0) pae[i] = 22.0;
            }

            BlockCreateInfo info;
            info.independent_specs.push_back(IndependentSpec{"Pin", std::move(pin_series), DimensionSpec::Regular(Np)});

            auto pout_ds = DataSeries::CreateScalarFromMemory<double>(pout.data(), Np);
            pout_ds.set_unit("W");
            auto gain_ds = DataSeries::CreateScalarFromMemory<double>(gain.data(), Np);
            gain_ds.set_unit("dB");
            auto pae_ds  = DataSeries::CreateScalarFromMemory<double>(pae.data(), Np);

            info.dependent_specs.push_back(DependentSpec{"Pout", std::move(pout_ds)});
            info.dependent_specs.push_back(DependentSpec{"Gain", std::move(gain_ds)});
            info.dependent_specs.push_back(DependentSpec{"PAE",  std::move(pae_ds)});
            ds.AddBlock("amplifier/HB1/HB", std::move(info));
        }

        // =====================================================================
        // Noise: freq 1..10 GHz, 10 steps
        // =====================================================================
        {
            const std::size_t Nf = 10;
            std::vector<double> freqs(Nf);
            for (std::size_t i = 0; i < Nf; ++i) freqs[i] = 1.0 + i * 1.0;

            DataSeries freq_series = DataSeries::CreateScalarFromMemory<double>(freqs.data(), Nf);
            freq_series.set_unit("GHz");

            std::vector<double> nf(Nf), rn(Nf);
            for (std::size_t i = 0; i < Nf; ++i)
            {
                double f = freqs[i];
                nf[i] = 0.8 + (f - 1.0) * 0.19;
                rn[i] = 15.0 + 2.0 * std::sin(f * 0.6);
            }

            BlockCreateInfo info;
            info.independent_specs.push_back(IndependentSpec{"freq", std::move(freq_series), DimensionSpec::Regular(Nf)});

            auto nf_ds = DataSeries::CreateScalarFromMemory<double>(nf.data(), Nf);
            nf_ds.set_unit("dB");
            auto rn_ds = DataSeries::CreateScalarFromMemory<double>(rn.data(), Nf);
            rn_ds.set_unit("Ohm");

            info.dependent_specs.push_back(DependentSpec{"NFmin", std::move(nf_ds)});
            info.dependent_specs.push_back(DependentSpec{"Rn",    std::move(rn_ds)});
            ds.AddBlock("amplifier/noise/nf", std::move(info));
        }

        // --- Save as HDF5 ---
        DatasetIO::Save(ds, "hdf5", "LNA_Design.h5");

        // --- Read back and verify structure ---
        Dataset loaded = DatasetIO::Load("hdf5", "LNA_Design.h5");
        EXPECT_EQ(loaded.name(), "LNA_Design");
        EXPECT_EQ(loaded.block_count(), 4u);

        EXPECT_TRUE(loaded.Exists("amplifier"));
        EXPECT_TRUE(loaded.Exists("amplifier/DC"));
        EXPECT_TRUE(loaded.Exists("amplifier/SP1"));
        EXPECT_TRUE(loaded.Exists("amplifier/HB1"));
        EXPECT_TRUE(loaded.Exists("amplifier/noise"));

        EXPECT_TRUE(loaded.IsLeaf("amplifier/DC/bias"));
        {
            const Block& b = loaded.GetBlock("amplifier/DC/bias");
            EXPECT_EQ(b.independents().size(), 2u);
            EXPECT_EQ(b.dependents().size(), 3u);
            EXPECT_EQ(b.dependent_spec("Id").data.size(), 99u);
            EXPECT_EQ(b.independent_spec("Vgs").data.unit().to_string(), "V");
            EXPECT_EQ(b.dependent_spec("gm").data.unit().to_string(), "S");

            double id_val = b.dependent_spec("Id").data.scalar_at<double>(4 * 11 + 3);
            double veff = 0.80 - 0.55;
            double expected_id = 0.12 * veff * veff * (1.0 + 0.04 * 3.0);
            EXPECT_NEAR(id_val, expected_id, 1e-9);
        }

        EXPECT_TRUE(loaded.IsLeaf("amplifier/SP1/SP"));
        {
            const Block& b = loaded.GetBlock("amplifier/SP1/SP");
            const DependentSpec& s_dep = b.dependent_spec("S");
            EXPECT_EQ(s_dep.data.data_shape()[0], 2);
            EXPECT_EQ(s_dep.data.data_shape()[1], 2);
            EXPECT_EQ(s_dep.data.size(), 10u);

            const auto& mat0 = s_dep.data.matrix_at<std::complex<double>>(0);
            double s21_db = 20.0 * std::log10(std::abs(mat0(1, 0)));
            EXPECT_NEAR(s21_db, 15.0, 0.5);
        }

        EXPECT_TRUE(loaded.IsLeaf("amplifier/HB1/HB"));
        EXPECT_TRUE(loaded.IsLeaf("amplifier/noise/nf"));

        std::cout << "HDF5 saved to LNA_Design.h5 (4 blocks)\n";
    }

} // namespace xdataset
