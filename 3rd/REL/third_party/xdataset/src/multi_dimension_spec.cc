#include "multi_dimension_spec.h"
#include <stdexcept>
#include <algorithm>
#include <functional>

namespace xdataset
{
    MultiDimensionSpec::MultiDimensionSpec()
        : dims_()
    {
    }

    MultiDimensionSpec& MultiDimensionSpec::add_regular(std::size_t size)
    {
        dims_.push_back(DimensionSpec::Regular(size));
        return *this;
    }

    MultiDimensionSpec& MultiDimensionSpec::add_ragged(const std::vector<std::size_t>& sizes)
    {
        return add_dimension(DimensionSpec::Ragged(sizes));
    }

    MultiDimensionSpec& MultiDimensionSpec::add_dimension(const DimensionSpec& dim)
    {
        if (dim.is_ragged() && !dims_.empty())   // skip first dim --  may be stored in isolation
        {
            const std::vector<std::size_t>& sizes = dim.as_ragged()->sizes;
            const std::size_t expected = compute_cell_count();
            if (sizes.size() != expected)
            {
                throw std::invalid_argument(
                    "ragged sizes count (" + std::to_string(sizes.size()) +
                    ") must match the total cell count of previous dimensions (" +
                    std::to_string(expected) + ")");
            }
        }
        dims_.push_back(dim);
        return *this;
    }

    std::size_t MultiDimensionSpec::rank() const
    {
        return dims_.size();
    }

    std::size_t MultiDimensionSpec::ndim() const
    {
        return rank();
    }

    bool MultiDimensionSpec::empty() const
    {
        return dims_.empty();
    }

    const std::vector<DimensionSpec>& MultiDimensionSpec::dims() const
    {
        return dims_;
    }

    const DimensionSpec& MultiDimensionSpec::dim(Index index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= dims_.size())
        {
            throw std::out_of_range("dimension index out of range");
        }
        return dims_[static_cast<std::size_t>(index)];
    }

    void MultiDimensionSpec::set_dimensions(const std::vector<DimensionSpec>& dims)
    {
        std::vector<DimensionSpec> old = std::move(dims_);
        dims_.clear();
        try
        {
            for (const auto& d : dims)
            {
                add_dimension(d);
            }
        }
        catch (...)
        {
            dims_ = std::move(old);
            throw;
        }
    }

    Index MultiDimensionSpec::flat_index(const std::vector<Index>& multi_index) const
    {
        if (multi_index.size() != dims_.size())
        {
            throw std::invalid_argument("multi_index size must match rank");
        }
        
        if (dims_.empty())
        {
            return 0;
        }

        Index flat = 0;
        for (std::size_t d = 0; d < dims_.size(); ++d)
        {
            if (dims_[d].is_regular())
            {
                // For uniform dimension: flat = flat * size_d + index_d (row-major order)
                flat = flat * static_cast<Index>(dims_[d].as_regular()->size) + multi_index[d];
            }
            else
            {
                // For Ragged dimension: flat is the parent row index, child is multi_index[d]
                const std::vector<std::size_t>& prefix = dims_[d].prefix_sum();
                flat = static_cast<Index>(prefix[static_cast<std::size_t>(flat)]) + multi_index[d];
            }
        }
        return flat;
    }

    std::vector<Index> MultiDimensionSpec::multi_index(Index flat) const
    {
        std::vector<Index> result(dims_.size(), 0);
        
        if (dims_.empty())
        {
            if (flat != 0)
            {
                throw std::out_of_range("flat index out of bounds");
            }
            return result;
        }

        Index residual = flat;
        for (std::size_t d = dims_.size(); d-- > 0;)
        {
            if (dims_[d].is_regular())
            {
                const Index size = static_cast<Index>(dims_[d].as_regular()->size);
                result[d] = residual % size;
                residual = residual / size;
            }
            else
            {
                // For Ragged: binary search in prefix sum to find the parent row,
                // then compute the child index within that parent.
                const std::vector<std::size_t>& prefix = dims_[d].prefix_sum();
                auto it = std::upper_bound(prefix.begin(), prefix.end(), static_cast<std::size_t>(residual));
                Index parent_row = static_cast<Index>(it - prefix.begin()) - 1;
                result[d] = residual - static_cast<Index>(prefix[static_cast<std::size_t>(parent_row)]);
                residual = parent_row;
            }
        }
        return result;
    }

    std::size_t MultiDimensionSpec::compute_cell_count() const
    {
        std::size_t count = 1;
        for (std::size_t i = 0; i < dims_.size(); ++i)
        {
            if (dims_[i].is_regular())
            {
                count *= dims_[i].as_regular()->size;
            }
            else
            {
                // For Ragged, directly set to total (no multiply, Ragged defines the structure)
                count = dims_[i].prefix_sum().back();
            }
        }
        return count;
    }

    void MultiDimensionSpec::for_each_leaf_row(const LeafRowVisitor& visitor) const
    {
        for_each_leaf_row(visitor, 0, static_cast<Index>(compute_cell_count()));
    }

    void MultiDimensionSpec::for_each_leaf_row(const LeafRowVisitor& visitor, Index start_flat_row, Index end_flat_row) const
    {
        if (!visitor)
        {
            return;
        }

        const Index total = static_cast<Index>(compute_cell_count());
        if (start_flat_row >= end_flat_row || start_flat_row >= total)
        {
            return;
        }
        if (end_flat_row > total)
        {
            end_flat_row = total;
        }

        if (dims_.empty())
        {
            LeafRow leaf_row;
            visitor(leaf_row);
            return;
        }

        const std::size_t rank = dims_.size();
        LeafRow leaf_row;
        leaf_row.multi_index.resize(rank);
        leaf_row.dimension_row_indices.resize(rank, 0);

        for (Index flat = start_flat_row; flat < end_flat_row; ++flat)
        {
            leaf_row.multi_index = multi_index(flat);
            leaf_row.row_flat = flat;

            // Compute dimension_row_indices in a single O(rank) pass.
            Index parent_flat = 0;
            for (std::size_t d = 0; d < rank; ++d)
            {
                if (dims_[d].is_regular())
                {
                    leaf_row.dimension_row_indices[d] = leaf_row.multi_index[d];
                    parent_flat = parent_flat * static_cast<Index>(dims_[d].regular_size())
                        + leaf_row.multi_index[d];
                }
                else
                {
                    Index start = 0;
                    Index end = 0;
                    dims_[d].child_range(parent_flat, start, end);
                    (void)end;
                    leaf_row.dimension_row_indices[d] = start + leaf_row.multi_index[d];
                    parent_flat = start + leaf_row.multi_index[d];
                }
            }

            visitor(leaf_row);
        }
    }

    std::size_t MultiDimensionSpec::group_count_at_dim(Index dim_idx) const
    {
        if (dim_idx < 0 || static_cast<std::size_t>(dim_idx) >= dims_.size())
        {
            return 0;
        }
        std::size_t count = 1;
        for (std::size_t i = 0; i <= static_cast<std::size_t>(dim_idx); ++i)
        {
            if (dims_[i].is_regular())
            {
                count *= dims_[i].as_regular()->size;
            }
            else
            {
                count = dims_[i].prefix_sum().back();
            }
        }
        return count;
    }

    void MultiDimensionSpec::for_each_group_at_dim(Index dim_idx, const DimGroupVisitor& visitor) const
    {
        if (!visitor || dims_.empty() || dim_idx < 0 || static_cast<std::size_t>(dim_idx) >= dims_.size())
        {
            return;
        }

        const Index total_cells = static_cast<Index>(compute_cell_count());
        const std::size_t rank  = dims_.size();

        // Phase 1: collect all groups with their multi_index and flat_start.
        std::vector<DimGroup> groups;
        std::vector<Index> full_mi(rank, 0);    // padded multi_index for flat_index() call
        std::vector<Index> mi_prefix(static_cast<std::size_t>(dim_idx) + 1, 0);

        std::function<void(std::size_t, Index)> walk =
            [&](std::size_t d, Index parent_flat)
        {
            std::size_t child_count = 0;
            if (dims_[d].is_regular())
            {
                child_count = dims_[d].regular_size();
            }
            else
            {
                child_count = dims_[d].child_width(parent_flat);
            }

            for (Index idx = 0; idx < static_cast<Index>(child_count); ++idx)
            {
                full_mi[d] = idx;

                Index current_flat = 0;
                if (dims_[d].is_regular())
                {
                    const Index size = static_cast<Index>(dims_[d].regular_size());
                    current_flat = parent_flat * size + idx;
                }
                else
                {
                    Index start = 0;
                    Index end = 0;
                    dims_[d].child_range(parent_flat, start, end);
                    (void)end;
                    current_flat = start + idx;
                }

                if (d < static_cast<std::size_t>(dim_idx))
                {
                    walk(d + 1, current_flat);
                    continue;
                }

                DimGroup g;
                g.multi_index.assign(full_mi.begin(), full_mi.begin() + static_cast<std::ptrdiff_t>(dim_idx + 1));
                g.flat_start = MultiDimensionSpec::flat_index(full_mi);  // padded with 0 for dims > dim_idx
                g.flat_end   = 0;  // fixed in phase 2
                groups.push_back(g);
            }
        };

        walk(0, 0);

        // Phase 2: fix up flat_end from adjacent groups, last capped at total_cells.
        for (std::size_t i = 0; i < groups.size(); ++i)
        {
            groups[i].flat_end = (i + 1 < groups.size()) ? groups[i + 1].flat_start : total_cells;
            visitor(groups[i]);
        }
    }

    std::string MultiDimensionSpec::to_string() const
    {
        std::string s = "[";
        for (std::size_t i = 0; i < dims_.size(); ++i)
        {
            if (i > 0) s += ", ";
            if (dims_[i].is_regular())
            {
                s += std::to_string(dims_[i].regular_size());
            }
            else
            {
                const auto& sizes = dims_[i].ragged_sizes();
                s += "[";
                for (std::size_t j = 0; j < sizes.size(); ++j)
                {
                    if (j > 0) s += ", ";
                    s += std::to_string(sizes[j]);
                }
                s += "]";
            }
        }
        s += "]";
        return s;
    }

} // namespace xdataset
