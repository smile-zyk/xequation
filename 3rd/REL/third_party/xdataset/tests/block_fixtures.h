#ifndef BLOCK_FIXTURES_H
#define BLOCK_FIXTURES_H

#include "block.h"

namespace xdataset
{
    namespace block_fixtures
    {
        // --- low-level helpers --------------------------------------------------

        inline DataSeries MakeScalarSeries(std::size_t size)
        {
            return DataSeries::CreateScalar<double>(size, Unit(), 0.0);
        }

        inline DataSeries MakeScalarSeriesFrom(const std::vector<double>& values)
        {
            return DataSeries::CreateScalarFromVector<double>(values);
        }

        inline DataSeries MakeIntScalarSeriesFrom(const std::vector<int>& values)
        {
            return DataSeries::CreateScalarFromVector<int>(values);
        }

        inline DataSeries MakeStringScalarSeriesFrom(const std::vector<std::string>& values)
        {
            DataSeries s = DataSeries::CreateScalar<std::string>(values.size());
            for (std::size_t i = 0; i < values.size(); ++i)
                s.scalar_at<std::string>(i) = values[i];
            return s;
        }

        inline DataSeries MakeVectorSeries(std::size_t rows, Index width)
        {
            DataSeries vecs(DataType::kReal, xdataset::DataShape::Vector(width));
            vecs.resize(rows);
            for (std::size_t i = 0; i < rows; ++i)
            {
                VecXd v(width);
                for (Index j = 0; j < width; ++j)
                    v(j) = static_cast<double>(i * width + j + 1);
                vecs.vector_at<double>(i) = v;
            }
            return vecs;
        }

        inline DataSeries MakeMatrixSeries(std::size_t rows, Index r, Index c)
        {
            DataSeries mats(DataType::kReal, xdataset::DataShape::Matrix(r, c));
            mats.resize(rows);
            for (std::size_t i = 0; i < rows; ++i)
            {
                xdataset::MatXd m(r, c);
                for (Index ri = 0; ri < r; ++ri)
                    for (Index ci = 0; ci < c; ++ci)
                        m(ri, ci) = static_cast<double>(i * r * c + ri * c + ci + 1);
                mats.matrix_at<double>(i) = m;
            }
            return mats;
        }

        // =====================================================================
        // Fixtures: uniform  (2 x 3)
        // =====================================================================

        inline BlockCreateInfo MakeBaseCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeries(2), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeries(3), DimensionSpec::Regular(3)});
            info.dependent_specs.push_back(
                DependentSpec{"z", MakeScalarSeries(6)});
            return info;
        }

        inline BlockCreateInfo MakeValueRichCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0, 3.0}), DimensionSpec::Regular(3)});
            info.dependent_specs.push_back(
                DependentSpec{"z", MakeScalarSeriesFrom({100.0, 101.0, 102.0, 103.0, 104.0, 105.0})});
            return info;
        }

        // Three independents (2 x 3 x 4 uniform), two dependents
        inline BlockCreateInfo MakeThreeDimMultiDepCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"a", MakeScalarSeriesFrom({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"b", MakeScalarSeriesFrom({10.0, 20.0, 30.0}), DimensionSpec::Regular(3)});
            info.independent_specs.push_back(
                IndependentSpec{"c", MakeScalarSeriesFrom({100.0, 200.0, 300.0, 400.0}), DimensionSpec::Regular(4)});
            info.dependent_specs.push_back(
                DependentSpec{"p", MakeScalarSeries(2 * 3 * 4)});
            info.dependent_specs.push_back(
                DependentSpec{"q", MakeScalarSeries(2 * 3 * 4)});
            return info;
        }

        // Single independent (edge case)
        inline BlockCreateInfo MakeSingleIndependentCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0, 30.0}), DimensionSpec::Regular(3)});
            info.dependent_specs.push_back(
                DependentSpec{"z", MakeScalarSeriesFrom({100.0, 200.0, 300.0})});
            return info;
        }

        // =====================================================================
        // Fixtures: string-typed  (2 x 2)
        // =====================================================================

        inline BlockCreateInfo MakeStringTypedCreateInfo()
        {
            BlockCreateInfo info;
            DataSeries xs = DataSeries::CreateScalar<std::string>(2);
            xs.scalar_at<std::string>(0) = "alpha";
            xs.scalar_at<std::string>(1) = "beta";
            info.independent_specs.push_back(
                IndependentSpec{"sx", std::move(xs), DimensionSpec::Regular(2)});

            DataSeries ys = DataSeries::CreateScalar<std::string>(2);
            ys.scalar_at<std::string>(0) = "one";
            ys.scalar_at<std::string>(1) = "two";
            info.independent_specs.push_back(
                IndependentSpec{"sy", std::move(ys), DimensionSpec::Regular(2)});

            info.dependent_specs.push_back(
                DependentSpec{"sz",
                    MakeStringScalarSeriesFrom({"A", "B", "C", "D"})});
            return info;
        }

        // =====================================================================
        // Fixtures: vector / matrix cell kinds  (2 x 2)
        // =====================================================================

        inline BlockCreateInfo MakeVectorCellCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back(
                DependentSpec{"vecs", MakeVectorSeries(4, 3)});
            return info;
        }

        inline BlockCreateInfo MakeMatrixCellCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back(
                DependentSpec{"mats", MakeMatrixSeries(4, 2, 2)});
            return info;
        }

        // =====================================================================
        // Fixtures: Ragged
        // =====================================================================

        inline BlockCreateInfo MakeRaggedCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.dependent_specs.push_back(
                DependentSpec{"z", MakeScalarSeriesFrom({100.0, 101.0, 102.0})});
            return info;
        }

        inline BlockCreateInfo MakeInterleavedCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.independent_specs.push_back(
                IndependentSpec{"z", MakeScalarSeriesFrom({100.0, 200.0}), DimensionSpec::Regular(2)});
            info.dependent_specs.push_back(
                DependentSpec{"w", MakeScalarSeriesFrom({1000.0, 1001.0, 1002.0, 1003.0, 1004.0, 1005.0})});
            return info;
        }

        // Ragged + vector dependent
        inline BlockCreateInfo MakeRaggedVectorCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"x", MakeScalarSeriesFrom({10.0, 20.0}), DimensionSpec::Regular(2)});
            info.independent_specs.push_back(
                IndependentSpec{"y", MakeScalarSeriesFrom({1.0, 2.0, 3.0}), DimensionSpec::Ragged({1, 2})});
            info.dependent_specs.push_back(
                DependentSpec{"v", MakeVectorSeries(3, 2)});
            return info;
        }

        // =====================================================================
        // Fixtures: dotted dependent names (e.g. SRC1.i, SRC1.v)
        // =====================================================================

        inline BlockCreateInfo MakeDottedDependentCreateInfo()
        {
            BlockCreateInfo info;
            info.independent_specs.push_back(
                IndependentSpec{"freq",
                    MakeScalarSeriesFrom({10.0, 20.0}),
                    DimensionSpec::Regular(2)});
            info.dependent_specs.push_back(
                DependentSpec{"SRC1.i",
                    MakeScalarSeriesFrom({1.0, 2.0})});
            info.dependent_specs.push_back(
                DependentSpec{"SRC1.v",
                    MakeScalarSeriesFrom({3.0, 4.0})});
            return info;
        }
    } // namespace block_fixtures
} // namespace xdataset

#endif // BLOCK_FIXTURES_H
