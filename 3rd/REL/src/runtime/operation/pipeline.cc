// =============================================================================
//  REL -- Operation pipeline (derive + operate)
// =============================================================================

#include "operation/pipeline.h"
#include "data_series.h"
#include "data_array.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace rel {
namespace operation {

using namespace xdataset;

// =========================================================================
//  RowBroadcastPlan::Compute
// =========================================================================

RowBroadcastPlan RowBroadcastPlan::Compute(const std::vector<Index>& sizes) {
    RowBroadcastPlan plan;
    Index r = 1;
    for (size_t i = 0; i < sizes.size(); ++i) {
        Index s = sizes[i];
        if (s == 1) continue;
        if (r == 1) { r = s; continue; }
        if (s != r)
            throw std::invalid_argument(
                "broadcast size mismatch (" + std::to_string(r) +
                " vs " + std::to_string(s) + ") at index " +
                std::to_string(i));
    }
    plan.result_size = r;
    for (size_t i = 0; i < sizes.size(); ++i)
        plan.broadcast.push_back(sizes[i] == 1 && r > 1);
    return plan;
}

// =========================================================================
//  ShapeBroadcastPlan::Make
// =========================================================================

ShapeBroadcastPlan ShapeBroadcastPlan::Make(const std::vector<DataShape>& operand_shapes,
                     const DataShape& result) {
    ShapeBroadcastPlan sp;
    DataKind rk = result.kind();
    sp.result_shape    = result;
    sp.result_elements = (rk == DataKind::kScalar) ? 1
                       : (rk == DataKind::kVector) ? result[0]
                       : result[0] * result[1];

    if (rk == DataKind::kMatrix)
        sp.result_cols = result[1];
    else if (rk == DataKind::kVector)
        sp.result_cols = result[0];
    else
        sp.result_cols = 1;

    for (size_t i = 0; i < operand_shapes.size(); ++i) {
        DataKind k = operand_shapes[i].kind();
        const auto& s = operand_shapes[i];

        OperandBroadcastShapeInfo op;
        op.elements = (k == DataKind::kScalar) ? 1
                    : (k == DataKind::kVector) ? s[0]
                    : s[0] * s[1];

        bool bc_row = false, bc_col = false;
        Index cols = 1;

        if (k == DataKind::kScalar) {
            bc_row = true;
            bc_col = true;
            cols   = 1;

        } else if (k == DataKind::kVector) {
            Index w = s[0];
            if (rk == DataKind::kVector) {
                bc_col = (w == 1 && sp.result_cols > 1);
                cols   = w;
            } else /* result is Matrix */ {
                bc_row = true;
                bc_col = (w == 1 && result[1] > 1);
                cols   = w;
            }

        } else /* kMatrix */ {
            cols   = s[1];
            bc_row = (s[0] == 1 && result[0] > 1);
            bc_col = (s[1] == 1 && result[1] > 1);
        }

        op.cols = cols;
        op.broadcast_row = bc_row;
        op.broadcast_col = bc_col;
        sp.ops.push_back(op);
    }

    return sp;
}

// =========================================================================
//  ShapeBroadcastPlan::MapFlatIndex
// =========================================================================

Index ShapeBroadcastPlan::MapFlatIndex(Index result_flat, int k) const {
    const OperandBroadcastShapeInfo& op = ops[static_cast<size_t>(k)];
    if (op.elements == 1) return 0;

    Index row = 0, col = result_flat;
    if (result_cols > 1 && result_elements != result_cols) {
        row = result_flat / result_cols;
        col = result_flat % result_cols;
    }

    Index r = op.broadcast_row ? 0 : row;
    Index c = op.broadcast_col ? 0 : col;
    return r * op.cols + c;
}

// =========================================================================
//  Operate
// =========================================================================

Value Operate(const std::vector<Value>& operands, const OpTraits& traits) {
    if (traits.arity != -1) {
        Index n = static_cast<Index>(operands.size());
        if (n != traits.arity) {
            throw std::invalid_argument(
                traits.name + ": arity mismatch: expected " +
                std::to_string(traits.arity) + " operand(s), got " +
                std::to_string(n));
        }
    }

    std::vector<Value> canonical_ops;
    canonical_ops.reserve(operands.size());
    for (size_t i = 0; i < operands.size(); ++i)
        canonical_ops.push_back(operands[i].canonicalized());

    std::vector<DataShape> operand_shapes;
    std::vector<Index>     row_counts;
    std::vector<DataType>  dtypes;
    std::vector<Unit>      units;

    for (size_t i = 0; i < canonical_ops.size(); ++i) {
        operand_shapes.push_back(canonical_ops[i].data_shape());
        row_counts.push_back(canonical_ops[i].rows());
        dtypes.push_back(canonical_ops[i].data_type());
        units.push_back(canonical_ops[i].unit());
    }

    try {
        DataShape shape = traits.derive_shape(operand_shapes);
        Index     rows  = traits.derive_rows(row_counts);
        DataType  dtype = traits.derive_dtype(dtypes);
        Unit      unit  = traits.derive_unit(units);

        ExecContextInfo info;
        info.rows  = rows;
        info.shape = shape;
        info.dtype = dtype;
        info.unit  = unit;

        return traits.execute(info, canonical_ops);
    } catch (const std::exception& e) {
        throw std::runtime_error(traits.name + ": " + e.what());
    }
}

}  // namespace operation
}  // namespace rel
