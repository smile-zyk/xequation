#include "block.h"
#include "dataset.h"
#include "dataset_io.h"
#include "unit.h"

#include <cmath>
#include <complex>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace xdataset;

// =============================================================================
// Helpers
// =============================================================================

namespace
{
    DataSeries doubles(const std::vector<double>& values, const Unit& u = Unit())
    {
        return DataSeries::CreateScalarFromVector<double>(values, u);
    }

    DataSeries vectors(std::size_t rows, Index width, const Unit& u = Unit())
    {
        DataSeries s(DataType::kReal, xdataset::DataShape::Vector(width));
        s.set_unit(u);
        s.resize(rows);
        for (std::size_t i = 0; i < rows; ++i)
            for (Index j = 0; j < width; ++j)
                s.vector_at<double>(static_cast<Index>(i))(j) = static_cast<double>(static_cast<Index>(i) * width + j + 1);
        return s;
    }

    DataSeries matrices(std::size_t rows, Index r, Index c, const Unit& u = Unit())
    {
        DataSeries s(DataType::kReal, xdataset::DataShape::Matrix(r, c));
        s.set_unit(u);
        s.resize(rows);
        for (std::size_t i = 0; i < rows; ++i)
            for (Index ri = 0; ri < r; ++ri)
                for (Index ci = 0; ci < c; ++ci)
                    s.matrix_at<double>(static_cast<Index>(i))(ri, ci) = static_cast<double>(static_cast<Index>(i) * r * c + ri * c + ci + 1);
        return s;
    }

    DataSeries strings(const std::vector<std::string>& vals, const Unit& u = Unit())
    {
        DataSeries s = DataSeries::CreateScalar<std::string>(vals.size(), u);
        for (std::size_t i = 0; i < vals.size(); ++i)
            s.scalar_at<std::string>(static_cast<Index>(i)) = vals[i];
        return s;
    }

    void print_row(const DataFrameRow& r, const std::string& label = "")
    {
        std::cout << "  " << label << r.FormatMultiIndex() << " |";
        for (const auto& f : r.fields)
            std::cout << " " << f.to_string();
        std::cout << std::endl;
    }

    void section(const std::string& title)
    {
        std::cout << "\n=== " << title << " ===" << std::endl;
    }
} // namespace

// =============================================================================
int main()
{
    try
    {
        // =====================================================================
        // 1. Simple regular block (2 x 3)
        // =====================================================================
        section("1. Simple 2x3 regular block");
        {
            auto m = Unit::parse("meter");
            auto s = Unit::parse("sec");
            auto V = Unit::parse("V");

            BlockCreateInfo info;
            info.independent_specs.push_back({"a", doubles({1.0, 2.0}, m), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"b", doubles({10.0, 20.0, 30.0}, s), DimensionSpec::Regular(3)});
            info.dependent_specs.push_back({"c", doubles({100.0, 101.0, 102.0, 103.0, 104.0, 105.0}, V)});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            std::cout << "headers: ";
            for (const auto& h : t.headers()) std::cout << h << " ";
            std::cout << "\nrows: " << t.row_count() << std::endl;
            // print first & last row
            print_row(t.GetRow(0), "first: ");
            print_row(t.GetRow(5), "last:  ");
            t.WriteToCsv("output/demo_simple.csv");
            std::cout << "-> written to demo_simple.csv" << std::endl;
        }

        // =====================================================================
        // 2. String-typed block
        // =====================================================================
        section("2. String-typed block");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"city", strings({"Paris", "London"}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"unit", strings({"kg", "L"}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"val", strings({"A", "B", "C", "D"})});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            print_row(t.GetRow(0));
            print_row(t.GetRow(1));
            print_row(t.GetRow(2));
            print_row(t.GetRow(3));
            t.WriteToCsv("output/demo_strings.csv");
            std::cout << "-> written to demo_strings.csv" << std::endl;
        }

        // =====================================================================
        // 3. Vector & matrix cell blocks
        // =====================================================================
        section("3. Vector cell block (2x2 -> 4 rows, vec width=3)");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"vec", vectors(4, 3, Unit::parse("W"))});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            for (const auto& h : t.headers()) std::cout << "  " << h;
            std::cout << std::endl;
            for (std::size_t i = 0; i < t.row_count(); ++i)
                print_row(t.GetRow(static_cast<Index>(i)));
            t.WriteToCsv("output/demo_vectors.csv");
            std::cout << "-> written to demo_vectors.csv" << std::endl;
        }

        section("4. Matrix cell block (2x2 -> 4 rows, mat 2x2)");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"mat", matrices(4, 2, 2, Unit::parse("meter"))});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            for (const auto& h : t.headers()) std::cout << "  " << h;
            std::cout << std::endl;
            for (std::size_t i = 0; i < t.row_count(); ++i)
                print_row(t.GetRow(static_cast<Index>(i)));
            t.WriteToCsv("output/demo_matrices.csv");
            std::cout << "-> written to demo_matrices.csv" << std::endl;
        }

        // =====================================================================
        // 5. Three-dimensional block with two dependents
        // =====================================================================
        section("5. 3D block (2x3x4) with two dependents");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"a", doubles({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"b", doubles({10.0, 20.0, 30.0}), DimensionSpec::Regular(3)});
            info.independent_specs.push_back({"c", doubles({100.0, 200.0, 300.0, 400.0}), DimensionSpec::Regular(4)});
            info.dependent_specs.push_back({"p", doubles(std::vector<double>(24, 0.0))});
            info.dependent_specs.push_back({"q", doubles(std::vector<double>(24, 0.0))});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            std::cout << "total rows: " << t.row_count() << ", columns: " << t.headers().size() << std::endl;
            print_row(t.GetRow(0),  "first:  ");
            print_row(t.GetRow(23), "last:   ");
            t.WriteToCsv("output/demo_3d.csv");
            std::cout << "-> written to demo_3d.csv" << std::endl;
        }

        // =====================================================================
        // 6. Ragged + interleaved (original demo)
        // =====================================================================
        section("6. Ragged-interleaved block (x x y(Ragged) x z)");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.independent_specs.push_back({"z", doubles({100.0, 200.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"w", doubles({1000.0, 1001.0, 1002.0, 1003.0, 1004.0, 1005.0})});
            info.dependent_specs.push_back({"v", vectors(6, 2, Unit::parse("Hz"))});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            for (const auto& h : t.headers()) std::cout << "  " << h;
            std::cout << std::endl;
            for (std::size_t i = 0; i < t.row_count(); ++i)
                print_row(t.GetRow(static_cast<Index>(i)));
            t.WriteToCsv("output/demo_jagged.csv");
            std::cout << "-> written to demo_jagged.csv" << std::endl;
        }

        // =====================================================================
        // 7. Lazy loading demo -- only access a few rows
        // =====================================================================
        section("7. Lazy loading -- only first 2 rows accessed from 2x3 grid");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0, 3.0}), DimensionSpec::Regular(3)});
            info.dependent_specs.push_back({"z", doubles({100.0, 101.0, 102.0, 103.0, 104.0, 105.0})});
            Block block(info);

            const DataFrame& t = block.GetOrCreateDataFrame();
            std::cout << "total row_count(): " << t.row_count() << " (but only 2 are loaded below)" << std::endl;
            print_row(t.GetRow(0));
            print_row(t.GetRow(1));
            // rows 2-5 are never accessed -- their chunks are never loaded
            std::cout << "(rows 2-5 were never materialised)" << std::endl;
        }

        // =====================================================================
        // 8. DataArray.indep() chain
        // =====================================================================
        section("8. DataArray.indep() chain");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.independent_specs.push_back({"z", doubles({100.0, 200.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"w", doubles({1000.0, 1001.0, 1002.0, 1003.0, 1004.0, 1005.0})});
            Block block(info);

            auto w = block.GetOrCreateDataArray("w");           // dependent
            std::cout << "w.data_kind() = " << (w.data_kind() == DataArrayKind::kDependent ? "dependent" : "independent") << std::endl;

            auto z_var = w.indep(1);
            std::cout << "w.indep(1) rank = " << z_var.multi_dimension_spec().rank() << std::endl;

            auto y_var = z_var.indep(2);
            std::cout << "z.indep(2) rank = " << y_var.multi_dimension_spec().rank() << std::endl;

            auto by_name = w.indep("y");
            std::cout << "w.indep(\"y\") rank = " << by_name.multi_dimension_spec().rank() << std::endl;

            // grid model from indep(1)
            std::cout << "\nGrid from w.indep(1):" << std::endl;
            const DataFrame& zt = z_var.GetOrCreateDataFrame();
            std::cout << "  headers: ";
            for (const auto& h : zt.headers()) std::cout << h << " ";
            std::cout << "\n  rows: " << zt.row_count() << std::endl;
            print_row(zt.GetRow(0));
            print_row(zt.GetRow(5));
        }

        // =====================================================================
        // 9. DataArray.select() demo
        // =====================================================================
        section("9. DataArray.select()");
        {
            BlockCreateInfo info;
            info.independent_specs.push_back({"x", doubles({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back({"y", doubles({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.independent_specs.push_back({"z", doubles({100.0, 200.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back({"w", doubles({1000.0, 1001.0, 1002.0, 1003.0, 1004.0, 1005.0})});
            Block block(info);

            auto w = block.GetOrCreateDataArray("w");

            // select where x=1, y=any, z=any -> collapses first dim
            auto sel = w.select({MultiIndexSelector::Equal(1),
                                  MultiIndexSelector::Any(),
                                  MultiIndexSelector::Any()});
            std::cout << "w.select(Equal(1), Any, Any) -> rank "
                      << sel.multi_dimension_spec().rank() << ", "
                      << sel.data().size() << " rows" << std::endl;
            const DataFrame& st = sel.GetOrCreateDataFrame();
            for (std::size_t i = 0; i < st.row_count(); ++i)
                print_row(st.GetRow(static_cast<Index>(i)));
        }

        // =====================================================================
        // 10. Measurement type inspection
        // =====================================================================
        section("10. Measurement type inspection");
        {
            Measurement d = Measurement::Scalar(3.14, Unit::parse("Ohm"));
            Measurement i = Measurement::Scalar(42, Unit::parse("F"));
            Measurement c = Measurement::Scalar(std::complex<double>(1.0, -2.0), Unit::parse("V"));
            Measurement s = Measurement::Scalar(std::string("hello"));

            std::cout << "Measurement(3.14 m/s):     "
                      << "  ToString=\"" << d.to_string() << "\"" << std::endl;
            std::cout << "Measurement(42 kg):        "
                      << "  ToString=\"" << i.to_string() << "\"" << std::endl;
            std::cout << "Measurement(1-2i V):       "
                      << "  ToString=\"" << c.to_string() << "\"" << std::endl;
            std::cout << "Measurement(\"hello\"):   "
                      << "  ToString=\"" << s.to_string() << "\"" << std::endl;

            std::cout << "Measurement(3.14).as_scalar<double>() = " << d.as_scalar<double>() << std::endl;
        }

        // =====================================================================
        // 12 . LNA simulation Dataset -> LNA_Design.h5
        // =====================================================================
        section("12. LNA simulation Dataset -> LNA_Design.h5");
        {
            Dataset ds("LNA_Design");

            // --- DC bias sweep: Vgs(11) x Vds(9) -> Id, gm, Vth ---
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
                auto id_ds = DataSeries::CreateScalarFromMemory<double>(id.data(), total); id_ds.set_unit("A");
                auto gm_ds = DataSeries::CreateScalarFromMemory<double>(gm_val.data(), total); gm_ds.set_unit("S");
                auto vth_ds = DataSeries::CreateScalarFromMemory<double>(vth_val.data(), total); vth_ds.set_unit("V");
                info.dependent_specs.push_back(DependentSpec{"Id.i", std::move(id_ds)});
                info.dependent_specs.push_back(DependentSpec{"gm", std::move(gm_ds)});
                info.dependent_specs.push_back(DependentSpec{"Vth", std::move(vth_ds)});
                ds.AddBlock("amplifier/DC/bias", std::move(info));
            }

            // --- S-parameters: freq(10) x 2x2 complex matrix ---
            {
                const std::size_t Nf = 10;
                std::vector<double> freqs(Nf);
                for (std::size_t i = 0; i < Nf; ++i) freqs[i] = 1.0 + i * 1.0;
                DataSeries freq_series = DataSeries::CreateScalarFromMemory<double>(freqs.data(), Nf);
                freq_series.set_unit("GHz");

                auto from_db = [](double db, double deg) -> std::complex<double> {
                    double m = std::pow(10.0, db / 20.0), r = deg * M_PI / 180.0;
                    return {m * std::cos(r), m * std::sin(r)};
                };

                std::vector<std::complex<double>> s_flat(Nf * 4);
                for (std::size_t i = 0; i < Nf; ++i)
                {
                    double f = freqs[i];
                    double s21_db = 15.0; if (f > 4.0) s21_db -= (f - 4.0) * 0.8;
                    std::size_t b = i * 4;
                    s_flat[b+0] = from_db(-12.0+(f-1.0)*0.65, -f*45.0);
                    s_flat[b+1] = from_db(-26.0+(f-1.0)*0.1,  90.0-f*5.0);
                    s_flat[b+2] = from_db(s21_db,              180.0-f*18.0);
                    s_flat[b+3] = from_db(-10.0+(f-1.0)*0.3,  -f*30.0);
                }

                BlockCreateInfo info;
                info.independent_specs.push_back(IndependentSpec{"freq", std::move(freq_series), DimensionSpec::Regular(Nf)});
                info.dependent_specs.push_back(DependentSpec{"S", DataSeries::CreateMatrixFromMemory<std::complex<double>>(2,2,s_flat.data(),s_flat.size())});
                ds.AddBlock("amplifier/SP1/SP", std::move(info));
            }

            // --- Harmonic balance: Pin(7) -> Pout, Gain, PAE ---
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
                    gain[i] = g; pout[i] = pin + g;
                    pae[i] = 2.0 + (pin + 20.0) * 0.90; if (pae[i] > 22.0) pae[i] = 22.0;
                }

                BlockCreateInfo info;
                info.independent_specs.push_back(IndependentSpec{"Pin", std::move(pin_series), DimensionSpec::Regular(Np)});
                auto pout_ds = DataSeries::CreateScalarFromMemory<double>(pout.data(), Np); pout_ds.set_unit("W");
                auto gain_ds = DataSeries::CreateScalarFromMemory<double>(gain.data(), Np); gain_ds.set_unit("dB");
                auto pae_ds  = DataSeries::CreateScalarFromMemory<double>(pae.data(), Np);
                info.dependent_specs.push_back(DependentSpec{"Pout", std::move(pout_ds)});
                info.dependent_specs.push_back(DependentSpec{"Gain", std::move(gain_ds)});
                info.dependent_specs.push_back(DependentSpec{"PAE", std::move(pae_ds)});
                ds.AddBlock("amplifier/HB1/HB", std::move(info));
            }

            // --- Noise: freq(10) -> NFmin, Rn ---
            {
                const std::size_t Nf = 10;
                std::vector<double> freqs(Nf);
                for (std::size_t i = 0; i < Nf; ++i) freqs[i] = 1.0 + i * 1.0;
                DataSeries freq_series = DataSeries::CreateScalarFromMemory<double>(freqs.data(), Nf);
                freq_series.set_unit("GHz");

                std::vector<double> nf(Nf), rn(Nf);
                for (std::size_t i = 0; i < Nf; ++i) { nf[i] = 0.8+(freqs[i]-1.0)*0.19; rn[i]=15.0+2.0*std::sin(freqs[i]*0.6); }

                BlockCreateInfo info;
                info.independent_specs.push_back(IndependentSpec{"freq", std::move(freq_series), DimensionSpec::Regular(Nf)});
                auto nf_ds = DataSeries::CreateScalarFromMemory<double>(nf.data(), Nf); nf_ds.set_unit("dB");
                auto rn_ds = DataSeries::CreateScalarFromMemory<double>(rn.data(), Nf); rn_ds.set_unit("Ohm");
                info.dependent_specs.push_back(DependentSpec{"NFmin", std::move(nf_ds)});
                info.dependent_specs.push_back(DependentSpec{"Rn", std::move(rn_ds)});
                ds.AddBlock("amplifier/noise/nf", std::move(info));
            }

            DatasetIO::Save(ds, "hdf5", "LNA_Design.h5");
            std::cout << "-> written to LNA_Design.h5 (" << ds.block_count() << " blocks)" << std::endl;

            // Quick CSV exports
            Dataset loaded = DatasetIO::Load("hdf5", "LNA_Design.h5");
            loaded.GetBlock("amplifier/DC/bias").GetOrCreateDataFrame().WriteToCsv("output/LNA_DC_bias.csv");
            loaded.GetBlock("amplifier/HB1/HB").GetOrCreateDataFrame().WriteToCsv("output/LNA_HB1_HB.csv");
            std::cout << "-> CSV exports: output/LNA_DC_bias.csv, output/LNA_HB1_HB.csv" << std::endl;
        }

        std::cout << "\nDone." << std::endl;
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
