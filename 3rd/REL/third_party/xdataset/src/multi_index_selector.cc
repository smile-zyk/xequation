#include "multi_index_selector.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace xdataset
{
    namespace
    {
        const char* SelectorKindName(MultiIndexSelector::Kind kind)
        {
            switch (kind)
            {
            case MultiIndexSelector::kAny:
                return "Any";
            case MultiIndexSelector::kEqual:
                return "Equal";
            case MultiIndexSelector::kIn:
                return "In";
            }

            return "Unknown";
        }
    } // namespace

    MultiIndexSelector MultiIndexSelector::Any()
    {
        return MultiIndexSelector(kAny, std::vector<Index>());
    }

    MultiIndexSelector MultiIndexSelector::Equal(Index idx)
    {
        if (idx == -1)
        {
            return Any();
        }
        if (idx < 0)
        {
            throw std::invalid_argument("equal selector index must be >= 0 or -1");
        }
        return MultiIndexSelector(kEqual, std::vector<Index>(1, idx));
    }

    MultiIndexSelector MultiIndexSelector::In(const std::vector<Index>& indices)
    {
        if (indices.empty())
        {
            throw std::invalid_argument("in selector must not be empty");
        }

        bool has_any = false;
        std::vector<Index> normalized;
        normalized.reserve(indices.size());

        for (std::size_t i = 0; i < indices.size(); ++i)
        {
            if (indices[i] == -1)
            {
                has_any = true;
                break;
            }
            if (indices[i] < 0)
            {
                throw std::invalid_argument("in selector index must be >= 0 or -1");
            }
            normalized.push_back(indices[i]);
        }

        if (has_any)
        {
            return Any();
        }

        std::sort(normalized.begin(), normalized.end());
        normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
        return MultiIndexSelector(kIn, normalized);
    }

    MultiIndexSelector::Kind MultiIndexSelector::kind() const
    {
        return kind_;
    }

    bool MultiIndexSelector::is_any() const
    {
        return kind_ == kAny;
    }

    bool MultiIndexSelector::is_equal() const
    {
        return kind_ == kEqual;
    }

    bool MultiIndexSelector::is_in() const
    {
        return kind_ == kIn;
    }

    Index MultiIndexSelector::equal_value() const
    {
        if (!is_equal())
        {
            throw std::logic_error("equal_value() is only valid for equal selectors");
        }
        return values_[0];
    }

    const std::vector<Index>& MultiIndexSelector::in_values() const
    {
        if (!is_in())
        {
            throw std::logic_error("in_values() is only valid for in selectors");
        }
        return values_;
    }

    std::vector<Index> MultiIndexSelector::resolve(Index width) const
    {
        if (width < 0)
        {
            throw std::logic_error(
                std::string("selector width must be non-negative: kind=") +
                SelectorKindName(kind_) + ", width=" + std::to_string(width));
        }

        std::vector<Index> selected;
        if (is_any())
        {
            selected.reserve(static_cast<std::size_t>(width));
            for (Index i = 0; i < width; ++i)
            {
                selected.push_back(i);
            }
            return selected;
        }

        if (is_equal())
        {
            const Index value = equal_value();
            if (value < 0 || value >= width)
            {
                throw std::out_of_range(
                    std::string("equal selector index out of range: value=") +
                    std::to_string(value) + ", width=" + std::to_string(width));
            }
            selected.push_back(value);
            return selected;
        }

        const std::vector<Index>& values = in_values();
        selected.reserve(values.size());
        for (Index value : values)
        {
            if (value < 0 || value >= width)
            {
                throw std::out_of_range(
                    std::string("in selector index out of range: value=") +
                    std::to_string(value) + ", width=" + std::to_string(width));
            }
            selected.push_back(value);
        }
        return selected;
    }

    bool MultiIndexSelector::matches(Index idx) const
    {
        if (kind_ == kAny)
        {
            return true;
        }
        if (kind_ == kEqual)
        {
            return idx == values_[0];
        }
        return std::binary_search(values_.begin(), values_.end(), idx);
    }

    MultiIndexSelector::MultiIndexSelector(Kind kind, const std::vector<Index>& values)
        : kind_(kind), values_(values)
    {
    }

} // namespace xdataset
