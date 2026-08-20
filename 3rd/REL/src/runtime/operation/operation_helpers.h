// =============================================================================
//  REL -- Internal operation helpers (shared by operator.cc & math_operation.cc)
// =============================================================================
//
//  This is a private header -- NOT part of the public API.  It contains:
//    - Shared templated execution loops (ExecBinaryLoop, ExecUnaryLoop, ...)
//    - Shared Derive callbacks (DeriveShapeBroadcast, DeriveDtypePromote, ...)

#ifndef REL_OPERATION_HELPERS_H
#define REL_OPERATION_HELPERS_H

#include "operation/pipeline.h"
#include "data_series.h"
#include "data_array.h"

#include <memory>
#include <vector>

namespace rel {
namespace operation {


// =========================================================================
//  Derive callbacks -- shape, rows, dtype, unit
// =========================================================================

xdataset::DataShape DeriveShapeBroadcast(const std::vector<xdataset::DataShape>& operand_shapes);
xdataset::Index    DeriveRowsBroadcast(const std::vector<xdataset::Index>& rows);

xdataset::DataType DeriveDtypePromote(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypePromoteReal(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypePromoteNoComplex(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypePromoteWithString(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypeRequireInt(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypeForceReal(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypeForceRealNoComplex(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypeForceIntNoComplex(const std::vector<xdataset::DataType>& dtypes);
xdataset::DataType DeriveDtypeComplexToReal(const std::vector<xdataset::DataType>& dtypes);

xdataset::Unit     DeriveUnitPromoteDimension(const std::vector<xdataset::Unit>& units);
xdataset::Unit     DeriveUnitMod(const std::vector<xdataset::Unit>& units);
xdataset::Unit     DeriveUnitDimlessRight(const std::vector<xdataset::Unit>& units);
xdataset::Unit     DeriveUnitMul(const std::vector<xdataset::Unit>& units);
xdataset::Unit     DeriveUnitDiv(const std::vector<xdataset::Unit>& units);
xdataset::Unit     DeriveUnitDimless(const std::vector<xdataset::Unit>& units);

// Simple constant-return derive callbacks
inline xdataset::DataType DeriveDtypeAlwaysInt(const std::vector<xdataset::DataType>&)  { return xdataset::DataType::kInteger; }
inline xdataset::DataType DeriveDtypeComplexOrReal(const std::vector<xdataset::DataType>& dtypes)
    { return (dtypes[0] == xdataset::DataType::kComplex) ? xdataset::DataType::kComplex : xdataset::DataType::kReal; }
inline xdataset::Unit DeriveUnitForceDimless(const std::vector<xdataset::Unit>&) { return xdataset::Unit(); }
inline xdataset::Unit DeriveUnitSquare(const std::vector<xdataset::Unit>& u) { return u[0].pow(2); }

// =========================================================================
//  ExecBinaryLoop -- core flat-buffer loop for binary ops
// =========================================================================
//
//  Row-level and cell-level broadcast are driven by the two plans.

template <typename T, typename Out = T>
inline void ExecBinaryLoop(xdataset::Index rows,
                            const RowBroadcastPlan& row_plan,
                            const ShapeBroadcastPlan& shape_plan,
                            const T* l_ptr, xdataset::Index l_stride,
                            const T* r_ptr, xdataset::Index r_stride,
                            Out* out,
                            Out (*elem_op)(T, T))
{
    xdataset::Index out_stride = shape_plan.result_elements;

    for (xdataset::Index i = 0; i < rows; ++i) {
        xdataset::Index l_row_off = (row_plan.broadcast[0] ? 0 : i) * l_stride;
        xdataset::Index r_row_off = (row_plan.broadcast[1] ? 0 : i) * r_stride;
        xdataset::Index o_off     = i * out_stride;

        for (xdataset::Index j = 0; j < shape_plan.result_elements; ++j) {
            xdataset::Index lj = shape_plan.MapFlatIndex(j, 0);
            xdataset::Index rj = shape_plan.MapFlatIndex(j, 1);
            out[o_off + j] = elem_op(
                l_ptr[l_row_off + lj],
                r_ptr[r_row_off + rj]);
        }
    }
}

// =========================================================================
//  ExecUnaryLoop -- core flat-buffer loop for single operand
// =========================================================================

template <typename T>
inline void ExecUnaryLoop(xdataset::Index rows,
                           const ShapeBroadcastPlan& shape_plan,
                           const T* ptr, xdataset::Index stride,
                           T* out,
                           UnaryOp<T> op)
{
    xdataset::Index out_stride = shape_plan.result_elements;

    for (xdataset::Index i = 0; i < rows; ++i) {
        xdataset::Index i_off = i * stride;
        xdataset::Index o_off = i * out_stride;

        for (xdataset::Index j = 0; j < shape_plan.result_elements; ++j) {
            out[o_off + j] = op(ptr[i_off + j]);
        }
    }
}

// =========================================================================
//  ExecBinaryArithT -- binary arithmetic entry point
// =========================================================================
//
//  Extract operand metadata, compute broadcast plans, flatten inputs,
//  allocate output, run the unified loop, and convert back to Value.

/// Binary ops: when the output is a DataArray, choose which operand's
/// metadata (MultiDimensionSpec, DataArrayKind) to inherit.
inline const xdataset::DataArray* SelectOutputSource(bool l_meas, bool r_meas,
                                            const std::vector<Value>& ops) {
    if (!l_meas && !r_meas) return &ops[0].as_data_array();
    if (l_meas && !r_meas) return &ops[1].as_data_array();
    if (!l_meas && r_meas) return &ops[0].as_data_array();
    return nullptr;
}

template <typename T>
inline Value ExecBinaryArithT(const ExecContextInfo& info,
                               const std::vector<Value>& ops,
                               ElemOp<T> elem_op)
{
    bool l_meas = ops[0].is_measurement();
    bool r_meas = ops[1].is_measurement();

    xdataset::DataShape l_shape = ops[0].data_shape();
    xdataset::DataShape r_shape = ops[1].data_shape();
    std::vector<xdataset::DataShape> op_shapes = {l_shape, r_shape};

    xdataset::Index l_rows = ops[0].rows();
    xdataset::Index r_rows = ops[1].rows();
    std::vector<xdataset::Index> row_counts = {l_rows, r_rows};

    ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
    RowBroadcastPlan   row_plan   = RowBroadcastPlan::Compute(row_counts);

    auto l_in    = ops[0].flat_data<T>();
    auto r_in    = ops[1].flat_data<T>();
    const T* l_ptr    = l_in.ptr;
    const T* r_ptr    = r_in.ptr;
    xdataset::Index    l_stride = l_in.stride;
    xdataset::Index    r_stride = r_in.stride;

    const xdataset::DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

    auto out_ds = std::unique_ptr<xdataset::DataSeries>(
        new xdataset::DataSeries(xdataset::DataTypeOf<T>::tag, info.shape));
    out_ds->set_unit(info.unit);
    out_ds->resize(static_cast<std::size_t>(info.rows));
    T* out = out_ds->mutable_contiguous_data<T>();

    ExecBinaryLoop(info.rows, row_plan, shape_plan,
                   l_ptr, l_stride, r_ptr, r_stride, out, elem_op);

    if (l_meas && r_meas) {
        return Value(out_ds->measurement_at(0));
    } else {
        auto da = std::make_shared<xdataset::DataArray>(out_src->clone());
        da->set_data(std::move(*out_ds));
        return Value(da);
    }
}

// =========================================================================
//  ExecUnaryT -- unary entry point (reuses flat_data, output helpers)
// =========================================================================

template <typename T>
inline Value ExecUnaryT(const ExecContextInfo& info,
                         const std::vector<Value>& ops,
                         UnaryOp<T> op)
{
    bool is_meas = ops[0].is_measurement();

    xdataset::DataShape op_shape = ops[0].data_shape();
    ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

    auto in = ops[0].flat_data<T>();
    const T* ptr    = in.ptr;
    xdataset::Index    stride = in.stride;

    auto out_ds = std::unique_ptr<xdataset::DataSeries>(
        new xdataset::DataSeries(xdataset::DataTypeOf<T>::tag, info.shape));
    out_ds->set_unit(info.unit);
    out_ds->resize(static_cast<std::size_t>(info.rows));
    T* out = out_ds->mutable_contiguous_data<T>();

    ExecUnaryLoop(info.rows, shape_plan, ptr, stride, out, op);

    if (is_meas) {
        return Value(out_ds->measurement_at(0));
    } else {
        const xdataset::DataArray& src = ops[0].as_data_array();
        auto da = std::make_shared<xdataset::DataArray>(src.clone());
        da->set_data(std::move(*out_ds));
        return Value(da);
    }
}

// =========================================================================
//  ExecUnaryCT -- unary entry point with input/output type change
//  (used for abs/real/imag/phase where complex input produces real output)
// =========================================================================

template <typename In, typename Out>
inline Value ExecUnaryCT(const ExecContextInfo& info,
                          const std::vector<Value>& ops,
                          Out (*op)(In))
{
    bool is_meas = ops[0].is_measurement();

    xdataset::DataShape op_shape = ops[0].data_shape();
    ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

    auto in = ops[0].flat_data<In>();
    const In* ptr = in.ptr;
    xdataset::Index stride = in.stride;

    auto out_ds = std::unique_ptr<xdataset::DataSeries>(
        new xdataset::DataSeries(xdataset::DataTypeOf<Out>::tag, info.shape));
    out_ds->set_unit(info.unit);
    out_ds->resize(static_cast<std::size_t>(info.rows));
    Out* out = out_ds->mutable_contiguous_data<Out>();

    xdataset::Index out_stride = shape_plan.result_elements;
    for (xdataset::Index i = 0; i < info.rows; ++i) {
        xdataset::Index i_off = i * stride;
        xdataset::Index o_off = i * out_stride;
        for (xdataset::Index j = 0; j < shape_plan.result_elements; ++j)
            out[o_off + j] = op(ptr[i_off + j]);
    }

    if (is_meas) {
        return Value(out_ds->measurement_at(0));
    } else {
        const xdataset::DataArray& src = ops[0].as_data_array();
        auto da = std::make_shared<xdataset::DataArray>(src.clone());
        da->set_data(std::move(*out_ds));
        return Value(da);
    }
}

}  // namespace operation
}  // namespace rel

#endif  // REL_OPERATION_HELPERS_H
