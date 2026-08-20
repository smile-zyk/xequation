#ifndef MULTI_DIMENSION_SPEC_H
#define MULTI_DIMENSION_SPEC_H

#include "dimension_spec.h"
#include "xdataset_predefine.h"
#include <functional>
#include <string>
#include <vector>

namespace xdataset
{
    class XDATASET_API MultiDimensionSpec
    {
    public:
        struct LeafRow
        {
            std::vector<Index> multi_index;
            std::vector<Index> dimension_row_indices;
            Index row_flat = 0;
        };

        using LeafRowVisitor = std::function<void(const LeafRow& leaf_row)>;

        MultiDimensionSpec();

        MultiDimensionSpec& add_regular(std::size_t size);
        
        MultiDimensionSpec& add_ragged(const std::vector<std::size_t>& sizes);
        
        MultiDimensionSpec& add_dimension(const DimensionSpec& dim);

        std::size_t rank() const;
        
        std::size_t ndim() const;
        
        bool empty() const;
        
        const std::vector<DimensionSpec>& dims() const;

        const DimensionSpec& dim(Index index) const;

        void set_dimensions(const std::vector<DimensionSpec>& dims);

        // CSR-based index conversion: multi-index to flat index
        Index flat_index(const std::vector<Index>& multi_index) const;
        
        // CSR-based reverse: flat index to multi-index
        std::vector<Index> multi_index(Index flat) const;

        // Compute total cell count based on current dimensions
        std::size_t compute_cell_count() const;

        // Visit each leaf row in row-major order and provide per-dimension source row positions.
        void for_each_leaf_row(const LeafRowVisitor& visitor) const;

        // Visit leaf rows whose flat index falls in [start_flat_row, end_flat_row).
        void for_each_leaf_row(const LeafRowVisitor& visitor, Index start_flat_row, Index end_flat_row) const;

        // -----------------------------------------------------------------------
        // Per-dimension-level group iteration
        // -----------------------------------------------------------------------
        //
        // A DimGroup represents all leaf rows under a single multi-index prefix
        // at a given dimension level.  Its [flat_start, flat_end) range can be
        // passed directly to for_each_leaf_row(visitor, flat_start, flat_end).
        //
        // Example: spec = [Regular(3), Regular(4), Regular(5)] (3x4x5 = 60 leaves)
        //   for_each_group_at_dim(0, ...) ->  3 groups, each spanning 20 leaves
        //   for_each_group_at_dim(1, ...) -> 12 groups, each spanning  5 leaves
        //   for_each_group_at_dim(2, ...) -> 60 groups (same as for_each_leaf_row)

        struct DimGroup
        {
            std::vector<Index> multi_index;  // prefix [0 .. dim_idx]
            Index              flat_start;   // first leaf flat index
            Index              flat_end;     // one-past-last leaf flat index (can pass to for_each_leaf_row)
        };

        using DimGroupVisitor = std::function<void(const DimGroup& group)>;

        std::size_t group_count_at_dim(Index dim_idx) const;

        void for_each_group_at_dim(Index dim_idx, const DimGroupVisitor& visitor) const;

        /// Render as a human-readable string, e.g. "[3, 4, 5]" or "[3, [2, 1, 2]]".
        std::string to_string() const;

    private:
        std::vector<DimensionSpec> dims_;
    };
} // namespace xdataset

#endif // MULTI_DIMENSION_SPEC_H
