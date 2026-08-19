#ifndef REL_RUNTIME_OPERATION_PIPELINE_H
#define REL_RUNTIME_OPERATION_PIPELINE_H

#include "rel_runtime_api.h"
#include "value.h"

#include <string>
#include <vector>

namespace rel {
namespace operation {

// =========================================================================
//  Broadcast plans
// =========================================================================

struct RowBroadcastPlan {
    xdataset::Index              result_size;
    std::vector<bool>  broadcast;

    static RowBroadcastPlan Compute(const std::vector<xdataset::Index>& sizes);
};

struct OperandBroadcastShapeInfo {
    xdataset::Index elements;
    xdataset::Index cols;
    bool  broadcast_row;
    bool  broadcast_col;
};

struct ShapeBroadcastPlan {
    xdataset::DataShape          result_shape;
    xdataset::Index              result_elements;
    xdataset::Index              result_cols;
    std::vector<OperandBroadcastShapeInfo> ops;

    static ShapeBroadcastPlan Make(const std::vector<xdataset::DataShape>& operand_shapes,
                                    const xdataset::DataShape& result);
    xdataset::Index MapFlatIndex(xdataset::Index result_flat, int k) const;
};

// =========================================================================
//  Execution context & traits
// =========================================================================

struct ExecContextInfo {
    xdataset::Index              rows;
    xdataset::DataShape          shape;
    xdataset::DataType           dtype;
    xdataset::Unit               unit;
};

template <typename T>
using ElemOp = T (*)(T, T);

template <typename T>
using UnaryOp = T (*)(T);

typedef xdataset::DataShape (*DeriveShapeFunc)(const std::vector<xdataset::DataShape>& operand_shapes);
typedef xdataset::DataType (*DeriveDtypeFunc)(const std::vector<xdataset::DataType>& dtypes);
typedef xdataset::Unit     (*DeriveUnitFunc)(const std::vector<xdataset::Unit>& units);
typedef xdataset::Index    (*DeriveRowsFunc)(const std::vector<xdataset::Index>& rows);
typedef Value    (*ExecuteFunc)(const ExecContextInfo& info,
                                const std::vector<Value>& ops);

struct OpTraits {
    xdataset::Index           arity;
    std::string               name;
    DeriveShapeFunc derive_shape;
    DeriveRowsFunc  derive_rows;
    DeriveDtypeFunc derive_dtype;
    DeriveUnitFunc  derive_unit;
    ExecuteFunc     execute;
};

// =========================================================================
//  Operate -- the core pipeline entry point
// =========================================================================

Value Operate(const std::vector<Value>& operands,
              const OpTraits& traits);

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_PIPELINE_H
