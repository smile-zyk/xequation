#include "dataset.h"
#include "block_fixtures.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace xdataset
{
    namespace
    {
        using namespace block_fixtures;

        BlockCreateInfo make_block_info(std::size_t indep_count = 2,
                                        std::size_t dep_count = 1)
        {
            std::size_t rows = 1;
            for (std::size_t i = 0; i < indep_count; ++i) rows *= 2;

            BlockCreateInfo info;
            for (std::size_t i = 0; i < indep_count; ++i)
            {
                info.independent_specs.push_back(
                    IndependentSpec{"iv" + std::to_string(i),
                                    MakeScalarSeries(2),
                                    DimensionSpec::Regular(2)});
            }
            for (std::size_t j = 0; j < dep_count; ++j)
            {
                info.dependent_specs.push_back(
                    DependentSpec{"dv" + std::to_string(j),
                                  MakeScalarSeries(rows)});
            }
            return info;
        }

    } // namespace

    // ========================================================================
    // Construction
    // ========================================================================

    TEST(DatasetTest, DefaultAndNamedConstruction)
    {
        Dataset ds;
        EXPECT_TRUE(ds.name().empty());

        Dataset named("noise");
        EXPECT_EQ(named.name(), "noise");

        named.set_name("other");
        EXPECT_EQ(named.name(), "other");
    }

    TEST(DatasetTest, EmptyDatasetHasNoBlocks)
    {
        Dataset ds("noise");
        EXPECT_EQ(ds.block_count(), 0u);
        EXPECT_TRUE(ds.GetAllBlockPaths().empty());
        EXPECT_TRUE(ds.GetBlockNames().empty());
        EXPECT_TRUE(ds.GetGroupNames().empty());
    }

    // ========================================================================
    // AddBlock
    // ========================================================================

    TEST(DatasetTest, AddBlockWithPathCreatesBlock)
    {
        Dataset ds("noise");
        Block& b = ds.AddBlock("SP1/SP", make_block_info());

        EXPECT_EQ(b.name(), "SP1.SP");
        EXPECT_TRUE(ds.IsLeaf("SP1/SP"));
        EXPECT_EQ(ds.block_count(), 1u);
    }

    TEST(DatasetTest, AddBlockTopLevel)
    {
        Dataset ds("noise");
        ds.AddBlock("results", make_block_info());

        EXPECT_EQ(ds.GetBlock("results").name(), "results");
        EXPECT_TRUE(ds.IsLeaf("results"));
        EXPECT_EQ(ds.block_count(), 1u);
    }

    TEST(DatasetTest, AddBlockMoveOverload)
    {
        Dataset ds("noise");
        BlockCreateInfo info = make_block_info();
        Block& b = ds.AddBlock("sim/SP", std::move(info));
        EXPECT_EQ(b.name(), "sim.SP");
    }

    TEST(DatasetTest, AddBlockDuplicatePathThrows)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        EXPECT_THROW({ ds.AddBlock("simulation/SP", make_block_info()); },
                     std::invalid_argument);
    }

    TEST(DatasetTest, AddBlockMultipleBlocksInSameGroup)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        ds.AddBlock("simulation/HB", make_block_info());

        EXPECT_TRUE(ds.IsLeaf("simulation/SP"));
        EXPECT_TRUE(ds.IsLeaf("simulation/HB"));
        EXPECT_EQ(ds.block_count(), 2u);
    }

    TEST(DatasetTest, AddBlockNestedCreatesIntermediateGroups)
    {
        Dataset ds("noise");
        ds.AddBlock("a/b/c/d", make_block_info());

        EXPECT_TRUE(ds.Exists("a"));
        EXPECT_TRUE(ds.Exists("a/b"));
        EXPECT_TRUE(ds.Exists("a/b/c"));
        EXPECT_TRUE(ds.IsLeaf("a/b/c/d"));
    }

    TEST(DatasetTest, AddBlockCollisionWithGroupNameThrows)
    {
        Dataset ds("noise");
        ds.AddBlock("a/b", make_block_info());
        EXPECT_THROW({ ds.AddBlock("a/b/c", make_block_info()); },
                     std::invalid_argument);
    }

    // ========================================================================
    // RemoveBlock / RemoveGroup
    // ========================================================================

    TEST(DatasetTest, RemoveBlockSucceeds)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        ds.AddBlock("simulation/HB", make_block_info());

        std::size_t removed = ds.RemoveBlock("simulation/SP");
        EXPECT_EQ(removed, 1u);
        EXPECT_FALSE(ds.IsLeaf("simulation/SP"));
        EXPECT_TRUE(ds.IsLeaf("simulation/HB"));
    }

    TEST(DatasetTest, RemoveBlockMissingPathReturnsZero)
    {
        Dataset ds("noise");
        EXPECT_EQ(ds.RemoveBlock("nonexistent"), 0u);
    }

    TEST(DatasetTest, RemoveBlockOnGroupNotBlockReturnsZero)
    {
        Dataset ds("noise");
        ds.AddBlock("a/b", make_block_info());
        EXPECT_EQ(ds.RemoveBlock("a"), 0u);
    }

    TEST(DatasetTest, RemoveGroupClearsSubtree)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP1/SP", make_block_info());
        ds.AddBlock("simulation/SP1/HB", make_block_info());
        ds.AddBlock("simulation/SP2/SP", make_block_info());

        std::size_t removed = ds.RemoveGroup("simulation/SP1");
        EXPECT_EQ(removed, 2u);
        EXPECT_FALSE(ds.Exists("simulation/SP1"));
        EXPECT_TRUE(ds.IsLeaf("simulation/SP2/SP"));
        EXPECT_EQ(ds.block_count(), 1u);
    }

    TEST(DatasetTest, RemoveGroupMissingPathReturnsZero)
    {
        Dataset ds("noise");
        EXPECT_EQ(ds.RemoveGroup("nonexistent"), 0u);
    }

    TEST(DatasetTest, RemoveGroupRootClearsAll)
    {
        Dataset ds("noise");
        ds.AddBlock("a", make_block_info());
        ds.AddBlock("b/c", make_block_info());

        std::size_t removed = ds.RemoveGroup("");
        EXPECT_EQ(removed, 2u);
        EXPECT_EQ(ds.block_count(), 0u);
    }

    // ========================================================================
    // IsLeaf / Exists / HasUniqueDataArray
    // ========================================================================

    TEST(DatasetTest, HasQueriesCorrect)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());

        EXPECT_TRUE(ds.Exists("simulation"));
        EXPECT_TRUE(ds.IsLeaf("simulation/SP"));
        EXPECT_FALSE(ds.IsLeaf("simulation/HB"));
        EXPECT_FALSE(ds.Exists("nonexistent"));
    }

    TEST(DatasetTest, ExistsOnRootReturnsTrue)
    {
        Dataset ds("noise");
        EXPECT_TRUE(ds.Exists(""));
    }

    TEST(DatasetTest, HasUniqueDataArrayTrue)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        EXPECT_TRUE(ds.HasUniqueDataArray("iv0"));
        EXPECT_TRUE(ds.HasUniqueDataArray("dv0"));
    }

    TEST(DatasetTest, HasUniqueDataArrayFalseWhenNone)
    {
        Dataset ds("noise");
        EXPECT_FALSE(ds.HasUniqueDataArray("nonexistent"));
    }

    TEST(DatasetTest, HasUniqueDataArrayFalseWhenMultiple)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        ds.AddBlock("simulation/HB", make_block_info());
        EXPECT_FALSE(ds.HasUniqueDataArray("iv0"));
    }

    // ========================================================================
    // GetDataArray
    // ========================================================================

    TEST(DatasetTest, GetDataArrayFullPath)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());

        const DataArray& da = ds.GetDataArray("simulation/SP", "iv0");
        EXPECT_EQ(da.data_kind(), DataArrayKind::kIndependent);
    }

    TEST(DatasetTest, GetDataArrayFullPathThrowsOnMissingBlock)
    {
        Dataset ds("noise");
        EXPECT_THROW({ ds.GetDataArray("nonexistent", "freq"); },
                     std::out_of_range);
    }

    TEST(DatasetTest, GetDataArrayFullPathThrowsOnMissingDataArray)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        EXPECT_THROW({ ds.GetDataArray("simulation/SP", "nonexistent"); },
                     std::invalid_argument);
    }

    TEST(DatasetTest, GetDataArrayUniqueNameShortcut)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        const DataArray& da = ds.GetDataArray("iv0");
    }

    TEST(DatasetTest, GetDataArrayUniqueNameThrowsOnNotFound)
    {
        Dataset ds("noise");
        EXPECT_THROW({ ds.GetDataArray("nonexistent"); },
                     std::invalid_argument);
    }

    TEST(DatasetTest, GetDataArrayUniqueNameThrowsOnAmbiguous)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());
        ds.AddBlock("simulation/HB", make_block_info());
        EXPECT_THROW({ ds.GetDataArray("iv0"); }, std::invalid_argument);
    }

    // ========================================================================
    // GetBlock
    // ========================================================================

    TEST(DatasetTest, GetBlockConst)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());

        const Dataset& cds = ds;
        const Block& b = cds.GetBlock("simulation/SP");
        EXPECT_EQ(b.name(), "simulation.SP");
    }

    TEST(DatasetTest, GetBlockThrowsOnMissing)
    {
        Dataset ds("noise");
        EXPECT_THROW({ ds.GetBlock("nonexistent"); }, std::out_of_range);
    }

    TEST(DatasetTest, GetBlockMutableAllowsLazyDataArrayCreation)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());

        Block& b = ds.GetBlock("simulation/SP");
        const DataArray& da = b.GetOrCreateDataArray("iv0");
    }

    // ========================================================================
    // GetDataArrayNames
    // ========================================================================

    TEST(DatasetTest, GetDataArrayNames)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP", make_block_info());

        std::vector<std::string> names = ds.GetDataArrayNames("simulation/SP");
        ASSERT_EQ(names.size(), 3u);
        EXPECT_EQ(names[0], "iv0");
        EXPECT_EQ(names[1], "iv1");
        EXPECT_EQ(names[2], "dv0");
    }

    TEST(DatasetTest, GetDataArrayNamesThrowsOnMissingBlock)
    {
        Dataset ds("noise");
        EXPECT_THROW({ ds.GetDataArrayNames("nonexistent"); }, std::out_of_range);
    }

    // ========================================================================
    // GetBlockNames / GetGroupNames / GetAllBlockPaths
    // ========================================================================

    TEST(DatasetTest, GetBlockNamesRoot)
    {
        Dataset ds("noise");
        ds.AddBlock("summary/stats", make_block_info());
        ds.AddBlock("simulation/SP1/SP", make_block_info());
        ds.AddBlock("simulation/SP1/HB", make_block_info());

        EXPECT_TRUE(ds.GetBlockNames().empty());

        auto names = ds.GetBlockNames("simulation/SP1");
        ASSERT_EQ(names.size(), 2u);
        EXPECT_EQ(names[0], "SP");
        EXPECT_EQ(names[1], "HB");
    }

    TEST(DatasetTest, GetGroupNames)
    {
        Dataset ds("noise");
        ds.AddBlock("simulation/SP1/SP", make_block_info());
        ds.AddBlock("summary/stats", make_block_info());

        auto names = ds.GetGroupNames();
        ASSERT_EQ(names.size(), 2u);
        EXPECT_EQ(names[0], "simulation");
        EXPECT_EQ(names[1], "summary");
    }

    TEST(DatasetTest, GetGroupNamesEmptyForLeaf)
    {
        Dataset ds("noise");
        ds.AddBlock("results", make_block_info());

        auto names = ds.GetGroupNames("results");
        EXPECT_TRUE(names.empty());
    }

    TEST(DatasetTest, GetBlockNamesThrowsOnMissing)
    {
        Dataset ds("noise");
        EXPECT_THROW({ ds.GetBlockNames("nonexistent"); }, std::out_of_range);
    }

    TEST(DatasetTest, GetAllBlockPathsInsertionOrder)
    {
        Dataset ds("noise");
        ds.AddBlock("b/block", make_block_info());
        ds.AddBlock("a/block", make_block_info());

        auto paths = ds.GetAllBlockPaths();
        ASSERT_EQ(paths.size(), 2u);
        EXPECT_EQ(paths[0], "b/block");
        EXPECT_EQ(paths[1], "a/block");
    }

    // ========================================================================
    // block_count
    // ========================================================================

    TEST(DatasetTest, BlockCountEmpty)
    {
        Dataset ds("noise");
        EXPECT_EQ(ds.block_count(), 0u);
    }

    TEST(DatasetTest, BlockCountWithNestedBlocks)
    {
        Dataset ds("noise");
        ds.AddBlock("a", make_block_info());
        ds.AddBlock("b/c", make_block_info());
        ds.AddBlock("b/d", make_block_info());

        EXPECT_EQ(ds.block_count(), 3u);
    }

    // ========================================================================
    // Dotted dependent names (e.g. SRC1.i, SRC1.v)
    // ========================================================================

    TEST(DatasetTest, GetDataArrayWithDottedDependentName)
    {
        Dataset ds("noise");
        ds.AddBlock("SP", MakeDottedDependentCreateInfo());

        const DataArray& i_data = ds.GetDataArray("SP", "SRC1.i");
        EXPECT_EQ(i_data.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(i_data.data().size(), 2u);

        const DataArray& v_data = ds.GetDataArray("SP", "SRC1.v");
        EXPECT_EQ(v_data.data_kind(), DataArrayKind::kDependent);
        EXPECT_EQ(v_data.data().size(), 2u);
    }

    TEST(DatasetTest, GetDataArrayNamesWithDottedDependents)
    {
        Dataset ds("noise");
        ds.AddBlock("SP", MakeDottedDependentCreateInfo());

        std::vector<std::string> names = ds.GetDataArrayNames("SP");
        ASSERT_EQ(names.size(), 3u);
        EXPECT_EQ(names[0], "freq");
        EXPECT_EQ(names[1], "SRC1.i");
        EXPECT_EQ(names[2], "SRC1.v");
    }

    TEST(DatasetTest, DottedDependentUniqueness)
    {
        Dataset ds("noise");
        ds.AddBlock("SP", MakeDottedDependentCreateInfo());

        EXPECT_TRUE(ds.HasUniqueDataArray("SRC1.i"));
        EXPECT_TRUE(ds.HasUniqueDataArray("SRC1.v"));
        EXPECT_TRUE(ds.HasUniqueDataArray("freq"));

        // Not unique because they're different names with same prefix
        EXPECT_FALSE(ds.HasUniqueDataArray("i"));
    }

} // namespace xdataset
