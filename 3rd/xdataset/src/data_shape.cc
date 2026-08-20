#include "data_shape.h"

namespace xdataset {

DataKind DataShape::kind() const
{
    if (dims.empty())    return DataKind::kScalar;
    if (dims.size() == 1) return DataKind::kVector;
    return DataKind::kMatrix;
}

Index DataShape::element_count() const
{
    if (dims.empty())    return 1;
    if (dims.size() == 1) return dims[0];
    return dims[0] * dims[1];
}

std::vector<Index> DataShape::copy() const
{
    return dims;
}

std::string DataShape::to_string() const
{
    switch (kind()) {
        case DataKind::kScalar: return "Scalar";
        case DataKind::kVector: return "Vector(" + std::to_string(dims[0]) + ")";
        case DataKind::kMatrix: return "Matrix(" + std::to_string(dims[0]) + ", " + std::to_string(dims[1]) + ")";
    }
    return "Unknown";
}

} // namespace xdataset
