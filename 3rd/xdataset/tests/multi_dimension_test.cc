#include "multi_dimension_spec.h"

#include <gtest/gtest.h>

namespace xdataset
{
    TEST(MultiDimensionSpecTest, TracksRankAndCellCount)
    {
        MultiDimensionSpec spec;
        EXPECT_TRUE(spec.empty());

        spec.add_regular(2).add_regular(3);

        EXPECT_FALSE(spec.empty());
        EXPECT_EQ(spec.rank(), 2u);
        EXPECT_EQ(spec.ndim(), 2u);
        ASSERT_EQ(spec.dims().size(), 2u);
        EXPECT_TRUE(spec.dims()[0].is_regular());
        EXPECT_TRUE(spec.dims()[1].is_regular());
        EXPECT_EQ(spec.compute_cell_count(), 6u);
    }

    TEST(MultiDimensionSpecTest, ForEachLeafRowReportsDimensionRowIndices)
    {
        MultiDimensionSpec spec;
        spec.add_regular(2).add_ragged({2, 1});

        std::vector<std::vector<Index>> multi_indices;
        std::vector<std::vector<Index>> dimension_row_indices;
        std::vector<Index> row_flats;

        spec.for_each_leaf_row(
            [&](const MultiDimensionSpec::LeafRow& leaf_row)
            {
                multi_indices.push_back(leaf_row.multi_index);
                dimension_row_indices.push_back(leaf_row.dimension_row_indices);
                row_flats.push_back(leaf_row.row_flat);
            });

        ASSERT_EQ(multi_indices.size(), 3u);
        ASSERT_EQ(dimension_row_indices.size(), 3u);
        ASSERT_EQ(row_flats.size(), 3u);

        EXPECT_EQ(multi_indices[0], std::vector<Index>({0, 0}));
        EXPECT_EQ(dimension_row_indices[0], std::vector<Index>({0, 0}));
        EXPECT_EQ(row_flats[0], 0);

        EXPECT_EQ(multi_indices[1], std::vector<Index>({0, 1}));
        EXPECT_EQ(dimension_row_indices[1], std::vector<Index>({0, 1}));
        EXPECT_EQ(row_flats[1], 1);

        EXPECT_EQ(multi_indices[2], std::vector<Index>({1, 0}));
        EXPECT_EQ(dimension_row_indices[2], std::vector<Index>({1, 2}));
        EXPECT_EQ(row_flats[2], 2);
    }

    TEST(MultiDimensionSpecTest, ForEachLeafRowHandlesInterleavedUniformAndRaggedDimensions)
    {
        MultiDimensionSpec spec;
        spec.add_regular(2).add_ragged({2, 1}).add_regular(2);

        std::vector<std::vector<Index>> multi_indices;
        std::vector<std::vector<Index>> dimension_row_indices;
        std::vector<Index> row_flats;

        spec.for_each_leaf_row(
            [&](const MultiDimensionSpec::LeafRow& leaf_row)
            {
                multi_indices.push_back(leaf_row.multi_index);
                dimension_row_indices.push_back(leaf_row.dimension_row_indices);
                row_flats.push_back(leaf_row.row_flat);
            });

        ASSERT_EQ(multi_indices.size(), 6u);
        ASSERT_EQ(dimension_row_indices.size(), 6u);
        ASSERT_EQ(row_flats.size(), 6u);

        EXPECT_EQ(multi_indices[0], std::vector<Index>({0, 0, 0}));
        EXPECT_EQ(dimension_row_indices[0], std::vector<Index>({0, 0, 0}));
        EXPECT_EQ(row_flats[0], 0);

        EXPECT_EQ(multi_indices[1], std::vector<Index>({0, 0, 1}));
        EXPECT_EQ(dimension_row_indices[1], std::vector<Index>({0, 0, 1}));
        EXPECT_EQ(row_flats[1], 1);

        EXPECT_EQ(multi_indices[2], std::vector<Index>({0, 1, 0}));
        EXPECT_EQ(dimension_row_indices[2], std::vector<Index>({0, 1, 0}));
        EXPECT_EQ(row_flats[2], 2);

        EXPECT_EQ(multi_indices[3], std::vector<Index>({0, 1, 1}));
        EXPECT_EQ(dimension_row_indices[3], std::vector<Index>({0, 1, 1}));
        EXPECT_EQ(row_flats[3], 3);

        EXPECT_EQ(multi_indices[4], std::vector<Index>({1, 0, 0}));
        EXPECT_EQ(dimension_row_indices[4], std::vector<Index>({1, 2, 0}));
        EXPECT_EQ(row_flats[4], 4);

        EXPECT_EQ(multi_indices[5], std::vector<Index>({1, 0, 1}));
        EXPECT_EQ(dimension_row_indices[5], std::vector<Index>({1, 2, 1}));
        EXPECT_EQ(row_flats[5], 5);
    }

    // =========================================================================
    // for_each_group_at_dim
    // =========================================================================

    TEST(MultiDimensionSpecTest, GroupCountAtDim)
    {
        MultiDimensionSpec spec;
        spec.add_regular(3).add_regular(4).add_regular(5);
        EXPECT_EQ(spec.rank(), 3u);
        EXPECT_EQ(spec.compute_cell_count(), 60u);

        EXPECT_EQ(spec.group_count_at_dim(0), 3u);    // 3 groups at dim 0
        EXPECT_EQ(spec.group_count_at_dim(1), 12u);   // 3*4=12 groups at dim 1
        EXPECT_EQ(spec.group_count_at_dim(2), 60u);   // 3*4*5=60 leaves at dim 2
    }

    TEST(MultiDimensionSpecTest, ForEachGroupAtDimUniform3x4x5)
    {
        // 3x4x5 regular grid: 60 leaves
        MultiDimensionSpec spec;
        spec.add_regular(3).add_regular(4).add_regular(5);

        // Dim 0: 3 groups, each spanning 20 leaves
        {
            std::vector<MultiDimensionSpec::DimGroup> groups;
            spec.for_each_group_at_dim(0, [&](const MultiDimensionSpec::DimGroup& g) {
                groups.push_back(g);
            });
            ASSERT_EQ(groups.size(), 3u);

            EXPECT_EQ(groups[0].multi_index, std::vector<Index>({0}));
            EXPECT_EQ(groups[0].flat_start, 0);
            EXPECT_EQ(groups[0].flat_end, 20);

            EXPECT_EQ(groups[1].multi_index, std::vector<Index>({1}));
            EXPECT_EQ(groups[1].flat_start, 20);
            EXPECT_EQ(groups[1].flat_end, 40);

            EXPECT_EQ(groups[2].multi_index, std::vector<Index>({2}));
            EXPECT_EQ(groups[2].flat_start, 40);
            EXPECT_EQ(groups[2].flat_end, 60);
        }

        // Dim 1: 12 groups, each spanning 5 leaves
        {
            std::vector<MultiDimensionSpec::DimGroup> groups;
            spec.for_each_group_at_dim(1, [&](const MultiDimensionSpec::DimGroup& g) {
                groups.push_back(g);
            });
            ASSERT_EQ(groups.size(), 12u);

            EXPECT_EQ(groups[0].multi_index, std::vector<Index>({0, 0}));
            EXPECT_EQ(groups[0].flat_start, 0);
            EXPECT_EQ(groups[0].flat_end, 5);

            EXPECT_EQ(groups[4].multi_index, std::vector<Index>({1, 0}));
            EXPECT_EQ(groups[4].flat_start, 20);
            EXPECT_EQ(groups[4].flat_end, 25);

            EXPECT_EQ(groups[11].multi_index, std::vector<Index>({2, 3}));
            EXPECT_EQ(groups[11].flat_start, 55);
            EXPECT_EQ(groups[11].flat_end, 60);
        }

        // Dim 2 (leaf level): 60 groups, each spanning 1 leaf (same as for_each_leaf_row)
        {
            std::vector<MultiDimensionSpec::DimGroup> groups;
            spec.for_each_group_at_dim(2, [&](const MultiDimensionSpec::DimGroup& g) {
                groups.push_back(g);
            });
            ASSERT_EQ(groups.size(), 60u);

            EXPECT_EQ(groups[0].multi_index, std::vector<Index>({0, 0, 0}));
            EXPECT_EQ(groups[0].flat_start, 0);
            EXPECT_EQ(groups[0].flat_end, 1);

            EXPECT_EQ(groups[59].multi_index, std::vector<Index>({2, 3, 4}));
            EXPECT_EQ(groups[59].flat_start, 59);
            EXPECT_EQ(groups[59].flat_end, 60);
        }
    }

    TEST(MultiDimensionSpecTest, ForEachGroupAtDimCanDriveForEachLeafRowRange)
    {
        // 3x4 regular grid: 12 leaves.  Access dim-1 groups to iterate leaves in
        // per-group batches of 4.
        MultiDimensionSpec spec;
        spec.add_regular(3).add_regular(4);

        std::vector<std::vector<Index>> all_multi_indices;
        spec.for_each_group_at_dim(0, [&](const MultiDimensionSpec::DimGroup& g) {
            // g.flat_start -> g.flat_end covers exactly one parent row's leaves
            spec.for_each_leaf_row(
                [&](const MultiDimensionSpec::LeafRow& lr) {
                    all_multi_indices.push_back(lr.multi_index);
                },
                g.flat_start, g.flat_end);
        });

        ASSERT_EQ(all_multi_indices.size(), 12u);
        EXPECT_EQ(all_multi_indices[0],  std::vector<Index>({0, 0}));
        EXPECT_EQ(all_multi_indices[3],  std::vector<Index>({0, 3}));
        EXPECT_EQ(all_multi_indices[4],  std::vector<Index>({1, 0}));
        EXPECT_EQ(all_multi_indices[11], std::vector<Index>({2, 3}));
    }

    TEST(MultiDimensionSpecTest, ForEachGroupAtDimRaggedInterleaved)
    {
        MultiDimensionSpec spec;
        spec.add_regular(2).add_ragged({1, 2}).add_regular(2);

        // Dim 0: 2 groups
        {
            std::vector<MultiDimensionSpec::DimGroup> groups;
            spec.for_each_group_at_dim(0, [&](const MultiDimensionSpec::DimGroup& g) {
                groups.push_back(g);
            });
            ASSERT_EQ(groups.size(), 2u);
            EXPECT_EQ(groups[0].multi_index, std::vector<Index>({0}));
            EXPECT_EQ(groups[0].flat_start, 0);
            EXPECT_EQ(groups[0].flat_end, 2);    // ragged node 0 has 1*2=2 leaves
            EXPECT_EQ(groups[1].multi_index, std::vector<Index>({1}));
            EXPECT_EQ(groups[1].flat_start, 2);
            EXPECT_EQ(groups[1].flat_end, 6);    // ragged node 1 has 2*2=4 leaves
        }

        // Dim 1: 1+2=3 groups
        {
            std::vector<MultiDimensionSpec::DimGroup> groups;
            spec.for_each_group_at_dim(1, [&](const MultiDimensionSpec::DimGroup& g) {
                groups.push_back(g);
            });
            ASSERT_EQ(groups.size(), 3u);
            EXPECT_EQ(groups[0].multi_index, std::vector<Index>({0, 0}));
            EXPECT_EQ(groups[0].flat_start, 0);
            EXPECT_EQ(groups[0].flat_end, 2);
            EXPECT_EQ(groups[1].multi_index, std::vector<Index>({1, 0}));
            EXPECT_EQ(groups[1].flat_start, 2);
            EXPECT_EQ(groups[1].flat_end, 4);
            EXPECT_EQ(groups[2].multi_index, std::vector<Index>({1, 1}));
            EXPECT_EQ(groups[2].flat_start, 4);
            EXPECT_EQ(groups[2].flat_end, 6);
        }
    }

} // namespace xdataset