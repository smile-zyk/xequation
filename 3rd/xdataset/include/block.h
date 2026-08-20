#ifndef BLOCK_H
#define BLOCK_H

#include <memory>
#include <string>
#include <tsl/ordered_map.h>
#include <vector>

#include "data_series.h"
#include "dimension_spec.h"
#include "data_frame.h"
#include "data_array.h"

namespace xdataset
{
    struct IndependentSpec
    {
        std::string   name;
        DataSeries    data;
        DimensionSpec dimension;
    };

    struct DependentSpec
    {
        std::string name;
        DataSeries  data;
    };

    struct BlockCreateInfo
    {
        std::vector<IndependentSpec> independent_specs;
        std::vector<DependentSpec>   dependent_specs;
    };

    // ========================================================================
    // Block -- leaf node in the Dataset tree
    // ========================================================================
    //
    // A Block holds the independent variables (coordinate axes) and dependent
    // variables (measurements) for one simulation result.  It is always a
    // LEAF in the Dataset tree -- Blocks do not contain other Blocks.
    //
    // Block.name() returns the short (leaf) name.
    // The full path within the Dataset is determined by the tree structure,
    // e.g. AddBlock("simulation/SP1/SP", info) -> Block::name() == "SP",
    // full path == "simulation/SP1/SP".
    //
    // ========================================================================
    class XDATASET_API Block
    {
    public:
        explicit Block(const BlockCreateInfo& info);
        explicit Block(BlockCreateInfo&& info);

        /// Short (leaf) name, e.g. "SP" for path "simulation/SP1/SP".
        const std::string& name() const;
        void               set_name(std::string name);

        std::vector<std::string> dependents() const;

        std::vector<std::string> independents() const;

        const IndependentSpec& independent_spec(const std::string& name) const;

        const DependentSpec& dependent_spec(const std::string& name) const;

        const DataArray& GetOrCreateDataArray(const std::string& name) const;
        const DataFrame& GetOrCreateDataFrame() const;

    private:
        DataArray CreateDataArray(const IndependentSpec& info) const;

        void ensure_unique_name(const std::string& name) const;

        std::string                                        name_;
        tsl::ordered_map<std::string, IndependentSpec> independent_spec_map_;
        tsl::ordered_map<std::string, DependentSpec>   dependent_spec_map_;
        mutable tsl::ordered_map<std::string, std::unique_ptr<DataArray>> data_array_cache_;
        mutable std::unique_ptr<DataFrame>                    data_frame_cache_;
    };
}

#endif  // BLOCK_H