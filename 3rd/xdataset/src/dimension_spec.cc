#include "dimension_spec.h"
#include <cstddef>
#include <stdexcept>

namespace xdataset
{
    // Factory methods
    DimensionSpec DimensionSpec::Regular(std::size_t size)
    {
        return DimensionSpec(RegularDim(size));
    }

    DimensionSpec DimensionSpec::Ragged(const std::vector<std::size_t>& sizes)
    {
        return DimensionSpec(RaggedDim(sizes));
    }

    // Constructors
    DimensionSpec::DimensionSpec(const RegularDim& regular)
        : spec_(regular)
    {
    }

    DimensionSpec::DimensionSpec(const RaggedDim& ragged)
        : spec_(ragged)
    {
    }

    // Type checking
    bool DimensionSpec::is_regular() const
    {
        return spec_.which() == 0;  // 0 = RegularDim, 1 = RaggedDim
    }

    bool DimensionSpec::is_ragged() const
    {
        return spec_.which() == 1;
    }

    // Safe access
    const RegularDim* DimensionSpec::as_regular() const
    {
        return boost::get<RegularDim>(&spec_);
    }

    const RaggedDim* DimensionSpec::as_ragged() const
    {
        return boost::get<RaggedDim>(&spec_);
    }

    // Convenient getters (with error checking)
    std::size_t DimensionSpec::regular_size() const
    {
        const RegularDim* u = as_regular();
        if (!u)
        {
            throw std::logic_error("regular_size() is only valid for regular dimensions");
        }
        return u->size;
    }

    const std::vector<std::size_t>& DimensionSpec::ragged_sizes() const
    {
        const RaggedDim* j = as_ragged();
        if (!j)
        {
            throw std::logic_error("ragged_sizes() is only valid for ragged dimensions");
        }
        return j->sizes;
    }

    const std::vector<std::size_t>& DimensionSpec::prefix_sum() const
    {
        const RaggedDim* j = as_ragged();
        if (j)
        {
            return j->prefix_sum;
        }
        static const std::vector<std::size_t> empty_vec;
        return empty_vec;
    }

    std::size_t DimensionSpec::element_count() const
    {
        if (is_regular())
        {
            return static_cast<std::size_t>(as_regular()->size);
        }
        else
        {
            return as_ragged()->sizes.size();
        }
    }

    std::size_t DimensionSpec::child_width(Index parent_idx) const
    {
        if (is_regular())
        {
            return as_regular()->size;
        }
        else
        {
            const RaggedDim* r = as_ragged();
            if (parent_idx < 0 || static_cast<std::size_t>(parent_idx) >= r->sizes.size())
            {
                throw std::out_of_range("parent_idx out of bounds");
            }
            return r->sizes[static_cast<std::size_t>(parent_idx)];
        }
    }

    void DimensionSpec::child_range(Index parent_idx, Index& start, Index& end) const
    {
        const RaggedDim* ragged = as_ragged();
        if (!ragged)
        {
            throw std::logic_error("child_range() is only valid for ragged dimensions");
        }
        if (parent_idx < 0 || static_cast<std::size_t>(parent_idx) >= ragged->sizes.size())
        {
            throw std::out_of_range("parent_idx out of bounds");
        }
        start = static_cast<Index>(ragged->prefix_sum[static_cast<std::size_t>(parent_idx)]);
        end = static_cast<Index>(ragged->prefix_sum[static_cast<std::size_t>(parent_idx) + 1]);
    }

} // namespace xdataset
