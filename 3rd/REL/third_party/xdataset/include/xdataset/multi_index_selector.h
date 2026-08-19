#ifndef MULTI_INDEX_SELECTOR_H_
#define MULTI_INDEX_SELECTOR_H_

#include "xdataset_predefine.h"
#include <vector>

namespace xdataset
{
    class XDATASET_API MultiIndexSelector
    {
    public:
        enum Kind
        {
            kAny,
            kEqual,
            kIn
        };

        static MultiIndexSelector Any();
        static MultiIndexSelector Equal(Index idx);
        static MultiIndexSelector In(const std::vector<Index>& indices);

        Kind kind() const;
        bool is_any() const;
        bool is_equal() const;
        bool is_in() const;
        Index equal_value() const;
        const std::vector<Index>& in_values() const;
        std::vector<Index> resolve(Index width) const;
        bool matches(Index idx) const;

    private:
        MultiIndexSelector(Kind kind, const std::vector<Index>& values);

        Kind kind_;
        std::vector<Index> values_;
    };

} // namespace xdataset

#endif // MULTI_INDEX_SELECTOR_H_