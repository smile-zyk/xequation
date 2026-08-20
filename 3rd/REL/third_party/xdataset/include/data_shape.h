#ifndef XDATASET_DATA_SHAPE_H_
#define XDATASET_DATA_SHAPE_H_

#include "xdataset_predefine.h"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace xdataset {

class XDATASET_API DataShape {
public:
    std::vector<Index> dims;

    DataShape() = default;
    DataShape(std::initializer_list<Index> il) : dims(il) {}
    explicit DataShape(const std::vector<Index>& v) : dims(v) {}

    // ------------------------------------------------------------------
    //  Named static constructors
    // ------------------------------------------------------------------

    static DataShape Scalar()                   { return DataShape{}; }
    static DataShape Vector(Index width)         { return DataShape{width}; }
    static DataShape Matrix(Index rows, Index cols) { return DataShape{rows, cols}; }

    // ------------------------------------------------------------------

    Index& operator[](size_t i)             { return dims[i]; }
    Index  operator[](size_t i) const       { return dims[i]; }
    size_t size()                     const { return dims.size(); }
    bool   empty()                    const { return dims.empty(); }
    void   clear()                          { dims.clear(); }
    void   push_back(Index v)               { dims.push_back(v); }

    bool operator==(const DataShape& o) const { return dims == o.dims; }
    bool operator!=(const DataShape& o) const { return dims != o.dims; }

    XDATASET_API DataKind kind() const;

    XDATASET_API Index element_count() const;

    XDATASET_API std::vector<Index> copy() const;

    XDATASET_API std::string to_string() const;
};

} // namespace xdataset

#endif // XDATASET_DATA_SHAPE_H_
