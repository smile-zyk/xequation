#include "block_fixtures.h"

#include <gtest/gtest.h>

namespace xdataset
{
    using namespace block_fixtures;

    TEST(BlockConstructorTest, BuildsSpecsFromIndependentAndDependentRules)
    {
        BlockCreateInfo info;

        IndependentSpec x{"x", MakeScalarSeries(2), DimensionSpec::Regular(2)};
        IndependentSpec y{"y", MakeScalarSeries(3), DimensionSpec::Regular(3)};
        DependentSpec z{"z", MakeScalarSeries(6)};

        info.independent_specs.push_back(x);
        info.independent_specs.push_back(y);
        info.dependent_specs.push_back(z);

        Block block(info);
        block.set_name("demo");

        ASSERT_EQ(block.name(), "demo");
        ASSERT_EQ(block.independents().size(), 2u);
        EXPECT_EQ(block.independents()[0], "x");
        EXPECT_EQ(block.independents()[1], "y");

        ASSERT_EQ(block.dependents().size(), 1u);
        EXPECT_EQ(block.dependents()[0], "z");

        const IndependentSpec& x_info = block.independent_spec("x");
        EXPECT_EQ(x_info.name, "x");
        EXPECT_TRUE(x_info.dimension.is_regular());
        EXPECT_EQ(x_info.dimension.regular_size(), 2u);

        // z is dependent -> independent_spec("z") should throw
        EXPECT_THROW({ block.independent_spec("z"); }, std::invalid_argument);
    }

    TEST(BlockConstructorTest, RejectsDependentWithoutIndependent)
    {
        BlockCreateInfo info;

        DependentSpec z{"z", MakeScalarSeries(1)};
        info.dependent_specs.push_back(z);

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

    TEST(BlockConstructorTest, RejectsDuplicateVariableNames)
    {
        BlockCreateInfo info;
        IndependentSpec x{"same", MakeScalarSeries(2), DimensionSpec::Regular(2)};
        DependentSpec z{"same", MakeScalarSeries(2)};

        info.independent_specs.push_back(x);
        info.dependent_specs.push_back(z);

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

    TEST(BlockConstructorTest, RejectsEmptyVariableName)
    {
        BlockCreateInfo info;
        IndependentSpec x{"", MakeScalarSeries(2), DimensionSpec::Regular(2)};

        info.independent_specs.push_back(x);

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

    TEST(BlockConstructorTest, RejectsDependentDataSizeMismatchWithDerivedDims)
    {
        BlockCreateInfo info;
        IndependentSpec x{"x", MakeScalarSeries(2), DimensionSpec::Regular(2)};
        IndependentSpec y{"y", MakeScalarSeries(3), DimensionSpec::Regular(3)};
        DependentSpec z{"z", MakeScalarSeries(5)};

        info.independent_specs.push_back(x);
        info.independent_specs.push_back(y);
        info.dependent_specs.push_back(z);

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

    TEST(BlockConstructorTest, RejectsIndependentDataSizeMismatchWithItsDim)
    {
        BlockCreateInfo info;
        IndependentSpec x{"x", MakeScalarSeries(3), DimensionSpec::Regular(2)};

        info.independent_specs.push_back(x);

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

    TEST(BlockVariableCacheTest, ReturnsSamePointerForCachedVariable)
    {
        Block block(MakeBaseCreateInfo());

        const DataArray& first = block.GetOrCreateDataArray("z");
        const DataArray& second = block.GetOrCreateDataArray("z");  EXPECT_EQ(&first, &second);
    }

    TEST(BlockVariableCacheTest, BuildsIndependentVariableWithPrefixDims)
    {
        Block block(MakeBaseCreateInfo());

        const DataArray& x_data = block.GetOrCreateDataArray("x"); EXPECT_EQ(x_data.data_kind(), DataArrayKind::kIndependent);
        ASSERT_EQ(x_data.multi_dimension_spec().rank(), 1u);
        ASSERT_EQ(x_data.multi_dimension_spec().dims().size(), 1u);
        EXPECT_EQ(x_data.multi_dimension_spec().dims()[0].regular_size(), 2);
        EXPECT_EQ(x_data.data().size(), 2u);

        const DataArray& y_data = block.GetOrCreateDataArray("y"); EXPECT_EQ(y_data.data_kind(), DataArrayKind::kIndependent);
        ASSERT_EQ(y_data.multi_dimension_spec().rank(), 2u);
        ASSERT_EQ(y_data.multi_dimension_spec().dims().size(), 2u);
        EXPECT_EQ(y_data.multi_dimension_spec().dims()[0].regular_size(), 2);
        EXPECT_EQ(y_data.multi_dimension_spec().dims()[1].regular_size(), 3);
        EXPECT_EQ(y_data.data().size(), 3u);   // raw: last-dim size
    }

    TEST(BlockVariableCacheTest, BuildsDependentVariableFromDescriptor)
    {
        Block block(MakeBaseCreateInfo());

        const DataArray& z_data = block.GetOrCreateDataArray("z"); EXPECT_EQ(z_data.data_kind(), DataArrayKind::kDependent);
        ASSERT_EQ(z_data.multi_dimension_spec().rank(), 2u);
        ASSERT_EQ(z_data.multi_dimension_spec().dims().size(), 2u);
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[0].regular_size(), 2);
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[1].regular_size(), 3);
        EXPECT_EQ(z_data.data().size(), 6u);
    }

    TEST(BlockVariableCacheTest, BuildsJaggedVariableWithPrefixDims)
    {
        Block block(MakeRaggedCreateInfo());

        const DataArray& y_data = block.GetOrCreateDataArray("y"); EXPECT_EQ(y_data.data_kind(), DataArrayKind::kIndependent);
        ASSERT_EQ(y_data.multi_dimension_spec().rank(), 2u);
        ASSERT_EQ(y_data.multi_dimension_spec().dims().size(), 2u);
        EXPECT_TRUE(y_data.multi_dimension_spec().dims()[0].is_regular());
        EXPECT_TRUE(y_data.multi_dimension_spec().dims()[1].is_ragged());
        EXPECT_EQ(y_data.multi_dimension_spec().dims()[0].regular_size(), 2);
        ASSERT_EQ(y_data.multi_dimension_spec().dims()[1].ragged_sizes().size(), 2u);
        EXPECT_EQ(y_data.multi_dimension_spec().dims()[1].ragged_sizes()[0], 1);
        EXPECT_EQ(y_data.multi_dimension_spec().dims()[1].ragged_sizes()[1], 2);
        EXPECT_EQ(y_data.data().size(), 3u);
    }

    TEST(BlockVariableCacheTest, BuildsInterleavedJaggedVariableWithPrefixDims)
    {
        Block block(MakeInterleavedCreateInfo());

        const DataArray& z_data = block.GetOrCreateDataArray("z"); EXPECT_EQ(z_data.data_kind(), DataArrayKind::kIndependent);
        ASSERT_EQ(z_data.multi_dimension_spec().rank(), 3u);
        ASSERT_EQ(z_data.multi_dimension_spec().dims().size(), 3u);
        EXPECT_TRUE(z_data.multi_dimension_spec().dims()[0].is_regular());
        EXPECT_TRUE(z_data.multi_dimension_spec().dims()[1].is_ragged());
        EXPECT_TRUE(z_data.multi_dimension_spec().dims()[2].is_regular());
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[0].regular_size(), 2);
        ASSERT_EQ(z_data.multi_dimension_spec().dims()[1].ragged_sizes().size(), 2u);
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[1].ragged_sizes()[0], 1);
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[1].ragged_sizes()[1], 2);
        EXPECT_EQ(z_data.multi_dimension_spec().dims()[2].regular_size(), 2);
        EXPECT_EQ(z_data.data().size(), 2u);   // raw: last-dim size
    }

    TEST(BlockDataFrameTest, AggregatesAllVariablesIntoOneTable)
    {
        Block block(MakeValueRichCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 3u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "z");

        ASSERT_EQ(table.row_count(), 6u);
        EXPECT_EQ(table.GetRow(0).multi_index[0], 0u);
        EXPECT_EQ(table.GetRow(0).multi_index[1], 0u);
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "100");
        EXPECT_EQ(table.GetRow(5).multi_index[0], 1u);
        EXPECT_EQ(table.GetRow(5).multi_index[1], 2u);
        EXPECT_EQ(table.GetRow(5).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(5).fields[1].to_string(), "3");
        EXPECT_EQ(table.GetRow(5).fields[2].to_string(), "105");

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find(",x,y,z"), std::string::npos);
        EXPECT_NE(csv.find("\"1,2\",20,3,105"), std::string::npos);
    }

    TEST(BlockDataFrameTest, AggregatesJaggedVariablesIntoOneTable)
    {
        Block block(MakeRaggedCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 3u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "z");

        ASSERT_EQ(table.row_count(), 3u);

        EXPECT_EQ(table.GetRow(0).multi_index, std::vector<Index>({0, 0}));
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "100");

        EXPECT_EQ(table.GetRow(1).multi_index, std::vector<Index>({1, 0}));
        EXPECT_EQ(table.GetRow(1).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(1).fields[1].to_string(), "2");
        EXPECT_EQ(table.GetRow(1).fields[2].to_string(), "101");

        EXPECT_EQ(table.GetRow(2).multi_index, std::vector<Index>({1, 1}));
        EXPECT_EQ(table.GetRow(2).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(2).fields[1].to_string(), "3");
        EXPECT_EQ(table.GetRow(2).fields[2].to_string(), "102");

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find(",x,y,z"), std::string::npos);
        EXPECT_NE(csv.find("\"1,1\",20,3,102"), std::string::npos);
    }

    TEST(BlockDataFrameTest, AggregatesInterleavedJaggedVariablesIntoOneTable)
    {
        Block block(MakeInterleavedCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 4u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "z");
        EXPECT_EQ(table.headers()[3], "w");

        ASSERT_EQ(table.row_count(), 6u);

        EXPECT_EQ(table.GetRow(0).multi_index, std::vector<Index>({0, 0, 0}));
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "100");
        EXPECT_EQ(table.GetRow(0).fields[3].to_string(), "1 K");

        EXPECT_EQ(table.GetRow(1).multi_index, std::vector<Index>({0, 0, 1}));
        EXPECT_EQ(table.GetRow(1).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(1).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(1).fields[2].to_string(), "200");
        EXPECT_EQ(table.GetRow(1).fields[3].to_string(), "1.001 K");

        EXPECT_EQ(table.GetRow(2).multi_index, std::vector<Index>({1, 0, 0}));
        EXPECT_EQ(table.GetRow(2).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(2).fields[1].to_string(), "2");
        EXPECT_EQ(table.GetRow(2).fields[2].to_string(), "100");
        EXPECT_EQ(table.GetRow(2).fields[3].to_string(), "1.002 K");

        EXPECT_EQ(table.GetRow(3).multi_index, std::vector<Index>({1, 0, 1}));
        EXPECT_EQ(table.GetRow(3).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(3).fields[1].to_string(), "2");
        EXPECT_EQ(table.GetRow(3).fields[2].to_string(), "200");
        EXPECT_EQ(table.GetRow(3).fields[3].to_string(), "1.003 K");

        EXPECT_EQ(table.GetRow(4).multi_index, std::vector<Index>({1, 1, 0}));
        EXPECT_EQ(table.GetRow(4).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(4).fields[1].to_string(), "3");
        EXPECT_EQ(table.GetRow(4).fields[2].to_string(), "100");
        EXPECT_EQ(table.GetRow(4).fields[3].to_string(), "1.004 K");

        EXPECT_EQ(table.GetRow(5).multi_index, std::vector<Index>({1, 1, 1}));
        EXPECT_EQ(table.GetRow(5).fields[0].to_string(), "20");
        EXPECT_EQ(table.GetRow(5).fields[1].to_string(), "3");
        EXPECT_EQ(table.GetRow(5).fields[2].to_string(), "200");
        EXPECT_EQ(table.GetRow(5).fields[3].to_string(), "1.005 K");

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find(",x,y,z,w"), std::string::npos);
        EXPECT_NE(csv.find("\"1,1,1\",20,3,200,1.005 K"), std::string::npos);
    }

    // =========================================================================
    // New fixtures: string, multi-dep, single-dim, vector, matrix
    // =========================================================================

    TEST(BlockDataFrameTest, AggregatesStringTypedVariables)
    {
        Block block(MakeStringTypedCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 3u);
        EXPECT_EQ(table.headers()[0], "sx");
        EXPECT_EQ(table.headers()[1], "sy");
        EXPECT_EQ(table.headers()[2], "sz");

        ASSERT_EQ(table.row_count(), 4u);

        EXPECT_EQ(table.GetRow(0).multi_index, std::vector<Index>({0, 0}));
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "alpha");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "one");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "A");

        EXPECT_EQ(table.GetRow(3).multi_index, std::vector<Index>({1, 1}));
        EXPECT_EQ(table.GetRow(3).fields[0].to_string(), "beta");
        EXPECT_EQ(table.GetRow(3).fields[1].to_string(), "two");
        EXPECT_EQ(table.GetRow(3).fields[2].to_string(), "D");

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find("sx,sy,sz"), std::string::npos);
        EXPECT_NE(csv.find("\"1,1\",beta,two,D"), std::string::npos);
    }

    TEST(BlockDataFrameTest, ThreeDimsWithMultipleDependents)
    {
        Block block(MakeThreeDimMultiDepCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 5u);
        EXPECT_EQ(table.headers()[0], "a");
        EXPECT_EQ(table.headers()[1], "b");
        EXPECT_EQ(table.headers()[2], "c");
        EXPECT_EQ(table.headers()[3], "p");
        EXPECT_EQ(table.headers()[4], "q");

        EXPECT_EQ(table.row_count(), 24u);
    }

    TEST(BlockDataFrameTest, SingleIndependentBehavesCorrectly)
    {
        Block block(MakeSingleIndependentCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 2u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "z");

        ASSERT_EQ(table.row_count(), 3u);
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "100");
        EXPECT_EQ(table.GetRow(2).fields[0].to_string(), "30");
        EXPECT_EQ(table.GetRow(2).fields[1].to_string(), "300");
    }

    TEST(BlockDataFrameTest, VectorCellColumnsAreExpanded)
    {
        Block block(MakeVectorCellCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 5u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "vecs(1)");
        EXPECT_EQ(table.headers()[3], "vecs(2)");
        EXPECT_EQ(table.headers()[4], "vecs(3)");

        ASSERT_EQ(table.row_count(), 4u);
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[3].to_string(), "2");
        EXPECT_EQ(table.GetRow(0).fields[4].to_string(), "3");
    }

    TEST(BlockDataFrameTest, MatrixCellColumnsAreExpanded)
    {
        Block block(MakeMatrixCellCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        ASSERT_EQ(table.headers().size(), 6u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "mats(1,1)");
        EXPECT_EQ(table.headers()[3], "mats(1,2)");
        EXPECT_EQ(table.headers()[4], "mats(2,1)");
        EXPECT_EQ(table.headers()[5], "mats(2,2)");

        ASSERT_EQ(table.row_count(), 4u);
        EXPECT_EQ(table.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(table.GetRow(0).fields[1].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[2].to_string(), "1");
        EXPECT_EQ(table.GetRow(0).fields[3].to_string(), "2");
        EXPECT_EQ(table.GetRow(0).fields[4].to_string(), "3");
        EXPECT_EQ(table.GetRow(0).fields[5].to_string(), "4");
    }

    TEST(BlockVariableCacheTest, MultiDependentVariablesAreBuiltCorrectly)
    {
        Block block(MakeThreeDimMultiDepCreateInfo());
        const DataArray& a = block.GetOrCreateDataArray("a");
        const DataArray& p = block.GetOrCreateDataArray("p");
        const DataArray& q = block.GetOrCreateDataArray("q");

        EXPECT_EQ(a.data_kind(), DataArrayKind::kIndependent);
        EXPECT_EQ(a.multi_dimension_spec().rank(), 1u);
        EXPECT_EQ(p.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(p.multi_dimension_spec().rank(), 3u);
        EXPECT_EQ(q.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(q.multi_dimension_spec().rank(), 3u);
    }
    TEST(BlockDataFrameTest, AggregatesMatrixTypedVariables)
    {
        Block block(MakeMatrixCellCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        // 2 scalars (x, y) + 1 matrix (mats) expanded to 2x2=4 columns = 6
        ASSERT_EQ(table.headers().size(), 6u);
        EXPECT_EQ(table.headers()[0], "x");
        EXPECT_EQ(table.headers()[1], "y");
        EXPECT_EQ(table.headers()[2], "mats(1,1)");
        EXPECT_EQ(table.headers()[3], "mats(1,2)");
        EXPECT_EQ(table.headers()[4], "mats(2,1)");
        EXPECT_EQ(table.headers()[5], "mats(2,2)");

        ASSERT_EQ(table.row_count(), 4u);
    }

    // =========================================================================
    // Dotted dependent names (e.g. SRC1.i, SRC1.v)
    // =========================================================================

    TEST(BlockConstructorTest, DottedDependentNames)
    {
        Block block(MakeDottedDependentCreateInfo());
        block.set_name("SP");

        ASSERT_EQ(block.independents().size(), 1u);
        EXPECT_EQ(block.independents()[0], "freq");

        ASSERT_EQ(block.dependents().size(), 2u);
        EXPECT_EQ(block.dependents()[0], "SRC1.i");
        EXPECT_EQ(block.dependents()[1], "SRC1.v");
    }

    TEST(BlockVariableCacheTest, GetOrCreateDataArrayWithDottedName)
    {
        Block block(MakeDottedDependentCreateInfo());

        const DataArray& i_data = block.GetOrCreateDataArray("SRC1.i");
        EXPECT_EQ(i_data.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(i_data.data().size(), 2u);

        const DataArray& v_data = block.GetOrCreateDataArray("SRC1.v");
        EXPECT_EQ(v_data.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(v_data.data().size(), 2u);

        // freq is independent
        const DataArray& freq_data = block.GetOrCreateDataArray("freq");
        EXPECT_EQ(freq_data.data_kind(), DataArrayKind::kIndependent);
    }

    TEST(BlockConstructorTest, RejectsDuplicateDottedAndFlatName)
    {
        BlockCreateInfo info;
        info.independent_specs.push_back(
            IndependentSpec{"freq", MakeScalarSeries(2), DimensionSpec::Regular(2)});
        info.dependent_specs.push_back(
            DependentSpec{"src.i", MakeScalarSeries(2)});
        info.dependent_specs.push_back(
            DependentSpec{"src.i", MakeScalarSeries(2)});  // duplicate

        EXPECT_THROW({ Block b(info); }, std::invalid_argument);
    }

} // namespace xdataset