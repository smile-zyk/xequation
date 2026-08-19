#ifndef DIMENSION_SPEC_H
#define DIMENSION_SPEC_H

#include "xdataset_predefine.h"
#include <boost/variant.hpp>
#include <cstddef>
#include <vector>

namespace xdataset
{
    struct RegularDim
    {
        std::size_t size;
        
        RegularDim() : size(0) {}
        explicit RegularDim(std::size_t s) : size(s) {}
    };

    struct RaggedDim
    {
        std::vector<std::size_t> sizes;
        std::vector<std::size_t> prefix_sum;
        
        RaggedDim() {}
        explicit RaggedDim(const std::vector<std::size_t>& s) : sizes(s)
        {
            compute_prefix_sum();
        }
        
        void compute_prefix_sum()
        {
            prefix_sum.clear();
            prefix_sum.reserve(sizes.size() + 1);
            prefix_sum.push_back(0);
            for (std::size_t i = 0; i < sizes.size(); ++i)
            {
                prefix_sum.push_back(prefix_sum.back() + static_cast<std::size_t>(sizes[i]));
            }
        }
    };

    class XDATASET_API DimensionSpec
    {
    public:
        static DimensionSpec Regular(std::size_t size);
        static DimensionSpec Ragged(const std::vector<std::size_t>& sizes);

        // Type checking
        bool is_regular() const;
        bool is_ragged() const;

        // Safe access: returns pointer if type matches, nullptr otherwise
        const RegularDim* as_regular() const;
        const RaggedDim* as_ragged() const;

        // Convenient getters (throws if wrong type)
        std::size_t regular_size() const;
        const std::vector<std::size_t>& ragged_sizes() const;
        const std::vector<std::size_t>& prefix_sum() const;
        
        // Number of elements at this level (regular: size, ragged: sizes.size())
        std::size_t element_count() const;
        
        // Width of child elements for a specific parent (regular: regular_size, ragged: ragged_sizes[parent_idx])
        std::size_t child_width(Index parent_idx) const;
        
        // Get child range [start, end) for parent at parent_idx (ragged only)
        void child_range(Index parent_idx, Index& start, Index& end) const;

    private:
        explicit DimensionSpec(const RegularDim& regular);
        explicit DimensionSpec(const RaggedDim& ragged);

        boost::variant<RegularDim, RaggedDim> spec_;
    };
} // namespace xdataset

#endif // DIMENSION_SPEC_H