// =============================================================================
//  xdataset -- operation framework
// =============================================================================
//
//  Pipeline:
//    Operate() derives result metadata (shape, rows, dtype, unit) from
//    operands, packs them into ExecContextInfo, then dispatches to the
//    execute callback registered in OpTraits.
//
//  The execute callback is responsible for performing the actual
//  computation.  For binary arithmetic, ExecBinaryArithT flattens
//  all operands to typed T* buffers, computes broadcast plans, runs
//  a unified loop, and converts the flat output back to Value.
//
//  Broadcast plans (RowBroadcastPlan, ShapeBroadcastPlan) are computed
//  inside the execute callbacks that need them; they do not appear in
//  the Operate pipeline.
//
//  This file also implements the Value / Measurement / DataArray operators
//  (delegating to the OperationXxx kernels below).  They used to live in
//  xdataset but now live in the rel library so that xdataset stays a
//  pure storage library.

#include "operation/operator.h"
#include "data_array.h"
#include "data_series.h"
#include "operation/operation_helpers.h"
#include "operation/pipeline.h"

#include <Eigen/LU>

#include <cmath>
#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

namespace rel
{

    using namespace xdataset;

    // =========================================================================
    //  Value operators (delegate to OperationXxx)
    // =========================================================================

    Value Value::operator+(const Value& rhs) const
    {
        return operation::OperationAdd(*this, rhs);
    }
    Value Value::operator-(const Value& rhs) const
    {
        return operation::OperationSub(*this, rhs);
    }
    Value Value::operator*(const Value& rhs) const
    {
        return operation::OperationMul(*this, rhs);
    }
    Value Value::operator/(const Value& rhs) const
    {
        return operation::OperationDiv(*this, rhs);
    }
    Value Value::operator%(const Value& rhs) const
    {
        return operation::OperationMod(*this, rhs);
    }

    Value Value::operator==(const Value& rhs) const
    {
        return operation::OperationEq(*this, rhs);
    }
    Value Value::operator!=(const Value& rhs) const
    {
        return operation::OperationNeq(*this, rhs);
    }
    Value Value::operator<(const Value& rhs) const
    {
        return operation::OperationLt(*this, rhs);
    }
    Value Value::operator>(const Value& rhs) const
    {
        return operation::OperationGt(*this, rhs);
    }
    Value Value::operator<=(const Value& rhs) const
    {
        return operation::OperationLe(*this, rhs);
    }
    Value Value::operator>=(const Value& rhs) const
    {
        return operation::OperationGe(*this, rhs);
    }

    Value Value::operator&&(const Value& rhs) const
    {
        return operation::OperationAnd(*this, rhs);
    }
    Value Value::operator||(const Value& rhs) const
    {
        return operation::OperationOr(*this, rhs);
    }

    Value Value::operator&(const Value& rhs) const
    {
        return operation::OperationBitAnd(*this, rhs);
    }
    Value Value::operator|(const Value& rhs) const
    {
        return operation::OperationBitOr(*this, rhs);
    }
    Value Value::operator^(const Value& rhs) const
    {
        return operation::OperationBitXor(*this, rhs);
    }
    Value Value::operator<<(const Value& rhs) const
    {
        return operation::OperationShl(*this, rhs);
    }
    Value Value::operator>>(const Value& rhs) const
    {
        return operation::OperationShr(*this, rhs);
    }

    Value Value::operator-() const
    {
        return operation::OperationNegate(*this);
    }
    Value Value::operator!() const
    {
        return operation::OperationNot(*this);
    }
    Value Value::operator~() const
    {
        return operation::OperationBitNot(*this);
    }

    Value Value::pow(const Value& exponent) const
    {
        return operation::OperationPow(*this, exponent);
    }

} // namespace rel

namespace rel
{
    namespace operation
    {

        // =========================================================================
        //  Operator-specific Derive callbacks
        // =========================================================================

        namespace
        {

            // -- Matrix helpers -----------------------------------------------------------

            std::pair<Index, Index> EffectiveRC(const DataShape& s)
            {
                DataKind k = s.kind();
                if (k == DataKind::kScalar)
                    return {1, 1};
                if (k == DataKind::kVector)
                    return {1, s[0]};
                return {s[0], s[1]};
            }

            DataShape MakeShapeRC(Index r, Index c)
            {
                if (r == 1 && c == 1)
                    return DataShape::Scalar();
                if (r == 1)
                    return DataShape::Vector(c);
                if (c == 1)
                    return DataShape::Vector(r);
                return DataShape::Matrix(r, c);
            }

            // -- Shape: Matrix, Mul, Div --------------------------------------------------

            static DataShape DeriveShapeMatrix(const std::vector<DataShape>& operand_shapes)
            {
                if (operand_shapes.empty())
                    throw std::invalid_argument("empty input");
                const Index N = static_cast<Index>(operand_shapes.size());
                const DataKind k0 = operand_shapes[0].kind();
                const DataShape& s0 = operand_shapes[0];
                for (size_t i = 1; i < operand_shapes.size(); ++i)
                {
                    if (operand_shapes[i].kind() != k0)
                        throw std::invalid_argument("kind mismatch at index " +
                                                    std::to_string(i));
                    if (operand_shapes[i] != s0)
                        throw std::invalid_argument("shape mismatch at index " +
                                                    std::to_string(i));
                }
                if (k0 == DataKind::kScalar)
                    return DataShape::Vector(N);
                if (k0 == DataKind::kVector)
                    return DataShape::Matrix(N, s0[0]);
                throw std::invalid_argument("cannot concat matrices");
            }

            static DataShape DeriveShapeMul(const std::vector<DataShape>& operand_shapes)
            {
                if (operand_shapes[0].kind() == DataKind::kScalar ||
                    operand_shapes[1].kind() == DataKind::kScalar)
                    return DeriveShapeBroadcast(operand_shapes);
                auto rcA = EffectiveRC(operand_shapes[0]);
                auto rcB = EffectiveRC(operand_shapes[1]);
                Index rA = rcA.first, cA = rcA.second;
                Index rB = rcB.first, cB = rcB.second;
                if (cA != rB)
                    throw std::invalid_argument("matmul inner dim mismatch: (" +
                                                std::to_string(rA) + "x" + std::to_string(cA) +
                                                ") x (" + std::to_string(rB) + "x" +
                                                std::to_string(cB) + ")");
                return MakeShapeRC(rA, cB);
            }

            static DataShape DeriveShapeDiv(const std::vector<DataShape>& operand_shapes)
            {
                if (operand_shapes[1].kind() == DataKind::kScalar)
                    return DeriveShapeBroadcast(operand_shapes);
                auto rcA = EffectiveRC(operand_shapes[0]);
                auto rcB = EffectiveRC(operand_shapes[1]);
                Index rA = rcA.first, cA = rcA.second;
                Index rB = rcB.first, cB = rcB.second;
                if (rB != cB)
                    throw std::invalid_argument("RHS matrix must be square for division (got " +
                                                std::to_string(rB) + "x" + std::to_string(cB) +
                                                ")");
                if (cA != cB)
                    throw std::invalid_argument("A(cols) must equal B(cols) for division: (" +
                                                std::to_string(rA) + "x" + std::to_string(cA) +
                                                ") / (" + std::to_string(rB) + "x" +
                                                std::to_string(cB) + ")");
                return MakeShapeRC(rA, rB);
            }

            // -- Rows: Sum -----------------------------------------------------------------

            static Index DeriveRowsSum(const std::vector<Index>& rows)
            {
                Index total = 0;
                for (size_t i = 0; i < rows.size(); ++i)
                    total += rows[i];
                return total;
            }

            // -- Conditional / If ---------------------------------------------------------

            static DataType DeriveDtypeConditional(const std::vector<DataType>& dtypes)
            {
                return DeriveDtypePromoteWithString({dtypes[1], dtypes[2]});
            }

            static Unit DeriveUnitConditional(const std::vector<Unit>& units)
            {
                return DeriveUnitPromoteDimension({units[1], units[2]});
            }

            static DataType DeriveDtypeIf(const std::vector<DataType>& dtypes)
            {
                std::vector<DataType> vals;
                for (size_t i = 1; i < dtypes.size(); ++i)
                {
                    if (i % 2 == 0 && i != dtypes.size() - 1) continue;
                    vals.push_back(dtypes[i]);
                }
                return DeriveDtypePromoteWithString(vals);
            }

            static Unit DeriveUnitIf(const std::vector<Unit>& units)
            {
                std::vector<Unit> val_units;
                for (size_t i = 1; i < units.size(); ++i)
                    if (i % 2 == 1 || i == units.size() - 1)
                        val_units.push_back(units[i]);
                return DeriveUnitPromoteDimension(val_units);
            }

        } // anonymous namespace

        // =========================================================================
        //  Element-wise operators
        // =========================================================================
        //
        //  Arithmetic, comparison, and logical element functions.  Each is
        //  instantiated for double, int, and std::complex<double>.

        namespace
        {

            template <typename T>
            inline T op_add(T a, T b)
            {
                return a + b;
            }
            template <typename T>
            inline T op_sub(T a, T b)
            {
                return a - b;
            }
            template <typename T>
            inline T op_mul(T a, T b)
            {
                return a * b;
            }
            template <typename T>
            inline T op_div(T a, T b)
            {
                return a / b;
            }
            template <typename T>
            inline T op_mod(T a, T b)
            {
                (void)a;
                (void)b;
                throw std::invalid_argument("mod not supported for this type");
            }
            template <>
            inline int op_mod<int>(int a, int b)
            {
                return a % b;
            }
            template <>
            inline double op_mod<double>(double a, double b)
            {
                return std::fmod(a, b);
            }

            template <typename T>
            inline T op_pow(T a, T b)
            {
                (void)a;
                (void)b;
                throw std::invalid_argument("pow not supported for this type");
            }
            template <>
            inline double op_pow<double>(double a, double b)
            {
                return std::pow(a, b);
            }
            template <>
            inline std::complex<double> op_pow<std::complex<double>>(std::complex<double> a,
                                                                     std::complex<double> b)
            {
                return std::pow(a, b);
            }

            // Numeric cmp: compare at actual type, return 0/1
            // complex uses abs() for < <= > >=
            template <typename T>
            inline int op_cmp_eq(T a, T b)
            {
                return a == b ? 1 : 0;
            }
            template <typename T>
            inline int op_cmp_ne(T a, T b)
            {
                return a != b ? 1 : 0;
            }
            template <typename T>
            inline int op_cmp_lt(T a, T b)
            {
                return a < b ? 1 : 0;
            }
            template <typename T>
            inline int op_cmp_gt(T a, T b)
            {
                return a > b ? 1 : 0;
            }
            template <typename T>
            inline int op_cmp_le(T a, T b)
            {
                return a <= b ? 1 : 0;
            }
            template <typename T>
            inline int op_cmp_ge(T a, T b)
            {
                return a >= b ? 1 : 0;
            }

            template <>
            inline int op_cmp_lt<std::complex<double>>(std::complex<double> a,
                                                       std::complex<double> b)
            {
                return std::abs(a) < std::abs(b) ? 1 : 0;
            }
            template <>
            inline int op_cmp_gt<std::complex<double>>(std::complex<double> a,
                                                       std::complex<double> b)
            {
                return std::abs(a) > std::abs(b) ? 1 : 0;
            }
            template <>
            inline int op_cmp_le<std::complex<double>>(std::complex<double> a,
                                                       std::complex<double> b)
            {
                return std::abs(a) <= std::abs(b) ? 1 : 0;
            }
            template <>
            inline int op_cmp_ge<std::complex<double>>(std::complex<double> a,
                                                       std::complex<double> b)
            {
                return std::abs(a) >= std::abs(b) ? 1 : 0;
            }

            // String cmp ->non-template to avoid copy overhead
            inline int str_cmp_eq(const std::string& a, const std::string& b)
            {
                return a == b ? 1 : 0;
            }
            inline int str_cmp_ne(const std::string& a, const std::string& b)
            {
                return a != b ? 1 : 0;
            }
            inline int str_cmp_lt(const std::string& a, const std::string& b)
            {
                return a < b ? 1 : 0;
            }
            inline int str_cmp_gt(const std::string& a, const std::string& b)
            {
                return a > b ? 1 : 0;
            }
            inline int str_cmp_le(const std::string& a, const std::string& b)
            {
                return a <= b ? 1 : 0;
            }
            inline int str_cmp_ge(const std::string& a, const std::string& b)
            {
                return a >= b ? 1 : 0;
            }

            template <typename T>
            inline T op_and(T a, T b)
            {
                return static_cast<T>((static_cast<int>(a) && static_cast<int>(b)) ? 1 : 0);
            }
            template <typename T>
            inline T op_or(T a, T b)
            {
                return static_cast<T>((static_cast<int>(a) || static_cast<int>(b)) ? 1 : 0);
            }

            template <typename T>
            inline T op_bitand(T /*a*/, T /*b*/)
            {
                throw std::invalid_argument("bitwise & not supported for this type");
            }
            template <>
            inline int op_bitand<int>(int a, int b)
            {
                return a & b;
            }

            template <typename T>
            inline T op_bitor(T /*a*/, T /*b*/)
            {
                throw std::invalid_argument("bitwise | not supported for this type");
            }
            template <>
            inline int op_bitor<int>(int a, int b)
            {
                return a | b;
            }

            template <typename T>
            inline T op_bitxor(T /*a*/, T /*b*/)
            {
                throw std::invalid_argument("bitwise ^ not supported for this type");
            }
            template <>
            inline int op_bitxor<int>(int a, int b)
            {
                return a ^ b;
            }

            template <typename T>
            inline T op_shl(T /*a*/, T /*b*/)
            {
                throw std::invalid_argument("shift << not supported for this type");
            }
            template <>
            inline int op_shl<int>(int a, int b)
            {
                return (b >= 0) ? (a << b) : (a >> (-b));
            }

            template <typename T>
            inline T op_shr(T /*a*/, T /*b*/)
            {
                throw std::invalid_argument("shift >> not supported for this type");
            }
            template <>
            inline int op_shr<int>(int a, int b)
            {
                return (b >= 0) ? (a >> b) : (a << (-b));
            }

        } // anonymous namespace

        // =========================================================================
        //  Unary element ops
        // =========================================================================

        namespace
        {

            template <typename T>
            inline T op_negate(T a)
            {
                return -a;
            }

            template <typename T>
            inline T op_not(T /*a*/)
            {
                throw std::invalid_argument("logical not not supported for this type");
            }
            template <>
            inline int op_not<int>(int a)
            {
                return (a == 0) ? 1 : 0;
            }

            template <typename T>
            inline T op_bitnot(T /*a*/)
            {
                throw std::invalid_argument("bitwise not not supported for this type");
            }
            template <>
            inline int op_bitnot<int>(int a)
            {
                return ~a;
            }

        } // anonymous namespace

        // =========================================================================
        //  Per-op execute callbacks -- binary arithmetic
        // =========================================================================

        Value ExecuteAdd(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        info, ops, op_add<std::complex<double>>);
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_add<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_add<int>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecuteSub(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        info, ops, op_sub<std::complex<double>>);
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_sub<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_sub<int>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecuteTimes(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        info, ops, op_mul<std::complex<double>>);
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_mul<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_mul<int>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecuteRdivide(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        info, ops, op_div<std::complex<double>>);
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_div<double>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecuteMod(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // Zero-divisor check for scalar Measurement / Measurement
            if (ops[0].is_measurement() && ops[1].is_measurement() &&
                info.shape.kind() == DataKind::kScalar)
            {
                switch (info.dtype)
                {
                    case DataType::kComplex:
                    {
                        auto r_in = ops[1].flat_data<std::complex<double>>();
                        if (r_in.ptr[0] == std::complex<double>(0))
                            throw std::invalid_argument("division by zero");
                        break;
                    }
                    case DataType::kReal:
                    {
                        auto r_in = ops[1].flat_data<double>();
                        if (r_in.ptr[0] == 0.0)
                            throw std::invalid_argument("division by zero");
                        break;
                    }
                    case DataType::kInteger:
                    {
                        auto r_in = ops[1].flat_data<int>();
                        if (r_in.ptr[0] == 0)
                            throw std::invalid_argument("division by zero");
                        break;
                    }
                    default: break;
                }
            }
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        info, ops, op_mod<std::complex<double>>);
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_mod<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_mod<int>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecutePow(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            ExecContextInfo mod_info = info;
            if (ops[1].data_shape().kind() == DataKind::kScalar &&
                ops[1].data_type() == DataType::kInteger && ops[1].rows() == 1)
            {
                auto rhs_flat = ops[1].flat_data<int>();
                mod_info.unit = info.unit.pow(rhs_flat.ptr[0]);
            }
            switch (mod_info.dtype)
            {
                case DataType::kComplex:
                    return ExecBinaryArithT<std::complex<double>>(
                        mod_info, ops, op_pow<std::complex<double>>);
                case DataType::kReal:
                    return ExecBinaryArithT<double>(mod_info, ops, op_pow<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(mod_info, ops, op_pow<int>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecBinaryCmpT -- compare at type T, output int 0/1
        // =========================================================================

        template <typename T>
        Value ExecBinaryCmpT(const ExecContextInfo& info,
                             const std::vector<Value>& ops,
                             int (*elem_op)(T, T))
        {
            bool l_meas = ops[0].is_measurement();
            bool r_meas = ops[1].is_measurement();

            DataShape l_shape = ops[0].data_shape();
            DataShape r_shape = ops[1].data_shape();
            std::vector<DataShape> op_shapes = {l_shape, r_shape};

            Index l_rows = ops[0].rows();
            Index r_rows = ops[1].rows();
            std::vector<Index> row_counts = {l_rows, r_rows};

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            auto l_in = ops[0].flat_data<T>();
            auto r_in = ops[1].flat_data<T>();
            const T* l_ptr = l_in.ptr;
            const T* r_ptr = r_in.ptr;
            Index l_stride = l_in.stride;
            Index r_stride = r_in.stride;

            const DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kInteger, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            int* out = out_ds->mutable_contiguous_data<int>();

            Index out_stride = shape_plan.result_elements;

            ExecBinaryLoop<T, int>(
                info.rows, row_plan, shape_plan, l_ptr, l_stride, r_ptr, r_stride, out, elem_op);

            if (l_meas && r_meas)
            {
                if (info.shape.kind() == DataKind::kScalar)
                    return Value::Boolean(out[0] != 0);

                return Value(out_ds->measurement_at(0));
            }
            else
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // -- String path for Cmp ---

        static Value ExecBinaryCmpString(const ExecContextInfo& info,
                                         const std::vector<Value>& ops,
                                         int (*elem_op)(const std::string&, const std::string&))
        {
            // Read strings directly; comparison at string type, output int
            bool l_meas = ops[0].is_measurement();
            bool r_meas = ops[1].is_measurement();

            DataShape l_shape = ops[0].data_shape();
            DataShape r_shape = ops[1].data_shape();
            std::vector<DataShape> op_shapes = {l_shape, r_shape};

            Index l_rows = ops[0].rows();
            Index r_rows = ops[1].rows();
            std::vector<Index> row_counts = {l_rows, r_rows};

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            // Build flat string arrays (no flat_data ->strings handled separately)
            Index l_stride = static_cast<Index>(l_shape.element_count());
            Index r_stride = static_cast<Index>(r_shape.element_count());
            Index result_rows = info.rows;
            Index out_stride = shape_plan.result_elements;

            // Pre-read all strings into flat vectors
            std::vector<std::string> l_flat, r_flat;
            auto read_flat =
                [](const Value& v, Index rows, Index stride, std::vector<std::string>& out)
            {
                out.resize(static_cast<std::size_t>(rows * stride));
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataKind dk = m.data_kind();
                    if (dk == DataKind::kScalar)
                    {
                        std::string s = m.as_scalar<std::string>();
                        for (Index i = 0; i < rows * stride; ++i)
                            out[static_cast<std::size_t>(i)] = s;
                    }
                    else if (dk == DataKind::kVector)
                    {
                        auto vec = m.as_vector<std::string>();
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] = vec(j);
                    }
                    else
                    {
                        auto mat = m.as_matrix<std::string>();
                        Index cols = m.shape()[1];
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    mat(j / cols, j % cols);
                    }
                }
                else
                {
                    const DataSeries& ds = v.as_data_array().data();
                    DataKind dk = ds.data_kind();
                    for (Index i = 0; i < rows; ++i)
                    {
                        Index src_row = i;
                        if (dk == DataKind::kScalar)
                            out[static_cast<std::size_t>(i * stride)] =
                                ds.scalar_at<std::string>(src_row);
                        else if (dk == DataKind::kVector)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.vector_at<std::string>(src_row)(j);
                        else
                        {
                            Index cols = ds.data_shape()[1];
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.matrix_at<std::string>(src_row)(j / cols, j % cols);
                        }
                    }
                }
            };
            read_flat(ops[0], l_rows, l_stride, l_flat);
            read_flat(ops[1], r_rows, r_stride, r_flat);

            // Compare strings directly

            const DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kInteger, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            int* out = out_ds->mutable_contiguous_data<int>();

            for (Index i = 0; i < info.rows; ++i)
            {
                Index l_row_off = (row_plan.broadcast[0] ? 0 : i) * l_stride;
                Index r_row_off = (row_plan.broadcast[1] ? 0 : i) * r_stride;
                Index o_off = i * out_stride;

                for (Index j = 0; j < shape_plan.result_elements; ++j)
                {
                    Index lj = shape_plan.MapFlatIndex(j, 0);
                    Index rj = shape_plan.MapFlatIndex(j, 1);
                    out[o_off + j] = elem_op(l_flat[static_cast<std::size_t>(l_row_off + lj)],
                                             r_flat[static_cast<std::size_t>(r_row_off + rj)]);
                }
            }

            if (l_meas && r_meas)
            {
                if (info.shape.kind() == DataKind::kScalar)
                    return Value::Boolean(out[0] != 0);
                return Value(out_ds->measurement_at(0));
            }
            else
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // =========================================================================
        //  Public execute callbacks -- Cmp and Logic
        // =========================================================================

        // =========================================================================
        //  Per-op execute callbacks -- comparison
        // =========================================================================

        Value ExecuteEq(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_eq);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_eq<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_eq<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_eq<int>);
            }
        }

        Value ExecuteNeq(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_ne);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_ne<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_ne<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_ne<int>);
            }
        }

        Value ExecuteLt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_lt);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_lt<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_lt<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_lt<int>);
            }
        }

        Value ExecuteGt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_gt);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_gt<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_gt<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_gt<int>);
            }
        }

        Value ExecuteLe(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_le);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_le<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_le<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_le<int>);
            }
        }

        Value ExecuteGe(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kString:
                    if (ops[0].data_type() != DataType::kString ||
                        ops[1].data_type() != DataType::kString)
                        throw std::invalid_argument("cannot mix string with numeric");
                    return ExecBinaryCmpString(info, ops, str_cmp_ge);
                case DataType::kComplex:
                    return ExecBinaryCmpT<std::complex<double>>(
                        info, ops, op_cmp_ge<std::complex<double>>);
                case DataType::kReal: return ExecBinaryCmpT<double>(info, ops, op_cmp_ge<double>);
                default: return ExecBinaryCmpT<int>(info, ops, op_cmp_ge<int>);
            }
        }

        // =========================================================================
        //  Per-op execute callbacks -- logical, bitwise, shift
        // =========================================================================

        // Logical ops helper: convert both operands to int via as_logical(),
        // apply element op, then optionally upgrade scalar Measurement to Boolean.
        static Value DoBinaryLogical(const ExecContextInfo& info,
                                     const std::vector<Value>& ops,
                                     ElemOp<int> elem_op)
        {
            auto make_logical = [](const Value& v) -> Value
            {
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataSeries ds(m.data_type(), m.shape());
                    ds.append(m);
                    auto logical_ds = std::unique_ptr<DataSeries>(new DataSeries(ds.as_logical()));
                    return Value(logical_ds->measurement_at(0));
                }
                else
                {
                    auto logical_ds = std::unique_ptr<DataSeries>(
                        new DataSeries(v.as_data_array().data().as_logical()));
                    auto da = std::make_shared<DataArray>(v.as_data_array().clone());
                    da->set_data(std::move(*logical_ds));
                    return Value(da);
                }
            };
            Value li = make_logical(ops[0]);
            Value ri = make_logical(ops[1]);

            Value result = ExecBinaryArithT<int>(info, {li, ri}, elem_op);

            if (ops[0].is_measurement() && ops[1].is_measurement() &&
                info.shape.kind() == DataKind::kScalar)
            {
                return Value::Boolean(result.as_measurement().as_scalar<int>() != 0);
            }
            return result;
        }

        Value ExecuteAnd(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return DoBinaryLogical(info, ops, op_and<int>);
        }

        Value ExecuteOr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return DoBinaryLogical(info, ops, op_or<int>);
        }

        Value ExecuteBitAnd(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<int>(info, ops, op_bitand<int>);
        }

        Value ExecuteBitOr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<int>(info, ops, op_bitor<int>);
        }

        Value ExecuteBitXor(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<int>(info, ops, op_bitxor<int>);
        }

        Value ExecuteShl(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<int>(info, ops, op_shl<int>);
        }

        Value ExecuteShr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<int>(info, ops, op_shr<int>);
        }

        // =========================================================================
        //  ExecuteMatrix ({} generator) - stack operands with row broadcast
        // =========================================================================
        //
        //  Output: all Measurement ->Measurement, otherwise DataArray.

        template <typename T>
        static Value ExecMatrixT(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            const Index N = static_cast<Index>(ops.size());

            bool all_meas = true;
            for (size_t i = 0; i < ops.size(); ++i)
                if (!ops[i].is_measurement())
                {
                    all_meas = false;
                    break;
                }

            std::vector<Index> row_counts;
            for (size_t i = 0; i < ops.size(); ++i)
                row_counts.push_back(ops[i].rows());
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            std::vector<Value::FlatData<T>> inputs;
            for (size_t i = 0; i < ops.size(); ++i)
                inputs.push_back(ops[i].flat_data<T>());

            Index cell_elems = static_cast<Index>(inputs[0].stride);
            Index result_rows = info.rows;

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(result_rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index row_stride = cell_elems * N;
            for (Index r = 0; r < result_rows; ++r)
            {
                Index out_off = r * row_stride;
                for (Index k = 0; k < N; ++k)
                {
                    Index op_row = (ops[static_cast<size_t>(k)].is_measurement())
                                       ? 0
                                       : (row_plan.broadcast[static_cast<size_t>(k)] ? 0 : r);
                    const T* src = inputs[static_cast<size_t>(k)].ptr +
                                   op_row * inputs[static_cast<size_t>(k)].stride;
                    for (Index j = 0; j < cell_elems; ++j)
                        out[out_off + k * cell_elems + j] = src[j];
                }
            }

            if (all_meas)
            {
                return Value(out_ds->measurement_at(0));
            }

            // Preserve metadata from first DataArray operand
            const DataArray* tmpl = nullptr;
            for (size_t i = 0; i < ops.size(); ++i)
                if (ops[i].is_data_array())
                {
                    tmpl = &ops[i].as_data_array();
                    break;
                }
            if (tmpl)
            {
                auto da = std::make_shared<DataArray>(tmpl->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }

            return Value(DataArray::CreateIndependent(std::move(*out_ds)));
        }

        // -- String path: no contiguous_data, access Measurement/DataSeries directly ---

        static Value ExecMatrixString(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            const Index N = static_cast<Index>(ops.size());

            bool all_meas = true;
            for (size_t i = 0; i < ops.size(); ++i)
                if (!ops[i].is_measurement())
                {
                    all_meas = false;
                    break;
                }

            std::vector<Index> row_counts;
            for (size_t i = 0; i < ops.size(); ++i)
                row_counts.push_back(ops[i].rows());
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            Index cell_elems = ops[0].data_shape().element_count();
            Index result_rows = info.rows;
            Index total = result_rows * cell_elems * N;

            // Build flat string vector, then construct final Measurement/DataSeries
            std::vector<std::string> flat(static_cast<std::size_t>(total));

            Index row_stride = cell_elems * N;
            for (Index r = 0; r < result_rows; ++r)
            {
                Index out_off = r * row_stride;
                for (Index k = 0; k < N; ++k)
                {
                    Index op_row = row_plan.broadcast[static_cast<size_t>(k)] ? 0 : r;
                    Index base = static_cast<Index>(static_cast<std::size_t>(out_off) +
                                                    static_cast<std::size_t>(k) *
                                                        static_cast<std::size_t>(cell_elems));

                    if (ops[static_cast<size_t>(k)].is_measurement())
                    {
                        const Measurement& m = ops[static_cast<size_t>(k)].as_measurement();
                        DataKind dk = m.data_kind();
                        if (dk == DataKind::kScalar)
                        {
                            std::string s = m.as_scalar<std::string>();
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = s;
                        }
                        else if (dk == DataKind::kVector)
                        {
                            auto vec = m.as_vector<std::string>();
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = vec(j);
                        }
                        else
                        {
                            auto mat = m.as_matrix<std::string>();
                            Index cols = m.shape()[1];
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = mat(j / cols, j % cols);
                        }
                    }
                    else
                    {
                        const DataSeries& ds = ops[static_cast<size_t>(k)].as_data_array().data();
                        DataKind dk = ds.data_kind();
                        if (dk == DataKind::kScalar)
                        {
                            std::string s = ds.scalar_at<std::string>(op_row);
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = s;
                        }
                        else if (dk == DataKind::kVector)
                        {
                            auto vec = ds.vector_at<std::string>(op_row);
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = vec(j);
                        }
                        else
                        {
                            auto mat = ds.matrix_at<std::string>(op_row);
                            Index cols = ds.data_shape()[1];
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = mat(j / cols, j % cols);
                        }
                    }
                }
            }

            if (all_meas)
            {
                // Single Measurement output
                DataKind dk = info.shape.kind();
                if (dk == DataKind::kVector)
                {
                    Index w = info.shape[0];
                    VecXs vec(w);
                    for (Index i = 0; i < w; ++i)
                        vec(i) = std::move(flat[static_cast<std::size_t>(i)]);
                    return Value::Vector(vec);
                }
                Index rows = info.shape[0], cols = info.shape[1];
                MatXs mat(rows, cols);
                for (Index i = 0; i < rows; ++i)
                    for (Index j = 0; j < cols; ++j)
                        mat(i, j) = std::move(flat[static_cast<std::size_t>(i * cols + j)]);
                return Value::Matrix(mat);
            }

            // DataArray output
            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kString, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(result_rows));
            for (Index r = 0; r < result_rows; ++r)
            {
                Index base = r * row_stride;
                if (info.shape.kind() == DataKind::kScalar)
                    out_ds->scalar_at<std::string>(r) =
                        std::move(flat[static_cast<std::size_t>(base)]);
                else if (info.shape.kind() == DataKind::kVector)
                    for (Index j = 0; j < info.shape[0]; ++j)
                        out_ds->vector_at<std::string>(r)(j) =
                            std::move(flat[static_cast<std::size_t>(base + j)]);
                else
                    for (Index i = 0; i < info.shape[0]; ++i)
                        for (Index j = 0; j < info.shape[1]; ++j)
                            out_ds->matrix_at<std::string>(r)(i, j) = std::move(
                                flat[static_cast<std::size_t>(base + i * info.shape[1] + j)]);
            }
            // DataArray output: preserve metadata from first DataArray operand
            const DataArray* tmpl = nullptr;
            for (size_t i = 0; i < ops.size(); ++i)
                if (ops[i].is_data_array())
                {
                    tmpl = &ops[i].as_data_array();
                    break;
                }
            if (tmpl)
            {
                auto da = std::make_shared<DataArray>(tmpl->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
            return Value(DataArray::CreateIndependent(std::move(*out_ds)));
        }

        Value ExecuteMatrix(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            if (ops.empty())
                throw std::invalid_argument("empty input");

            if (info.dtype == DataType::kString)
                return ExecMatrixString(info, ops);

            switch (info.dtype)
            {
                case DataType::kComplex: return ExecMatrixT<std::complex<double>>(info, ops);
                case DataType::kReal: return ExecMatrixT<double>(info, ops);
                case DataType::kInteger: return ExecMatrixT<int>(info, ops);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecuteSweep ([...] sweep generator) -- collect operands into DataArray
        // =========================================================================
        //
        //  RowBroadcastPlan handles row broadcast. ShapeBroadcastPlan handles cell
        //  broadcast (Scalar ->Vector etc.).

        template <typename T>
        static Value ExecSweepT(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            std::vector<DataShape> op_shapes;
            std::vector<Index> row_counts;
            for (size_t i = 0; i < ops.size(); ++i)
            {
                op_shapes.push_back(ops[i].data_shape());
                row_counts.push_back(ops[i].rows());
            }

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            std::vector<Value::FlatData<T>> inputs;
            for (size_t i = 0; i < ops.size(); ++i)
                inputs.push_back(ops[i].flat_data<T>());

            Index cell_elems = shape_plan.result_elements;
            Index result_rows = info.rows;

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(result_rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index out_row = 0;
            for (size_t k = 0; k < ops.size(); ++k)
            {
                Index n_rows = ops[k].rows();
                for (Index local_r = 0; local_r < n_rows; ++local_r, ++out_row)
                {
                    Index op_row =
                        (ops[k].is_measurement()) ? 0 : (row_plan.broadcast[k] ? 0 : local_r);
                    const T* src = inputs[k].ptr + op_row * inputs[k].stride;

                    if (inputs[k].stride == cell_elems)
                    {
                        for (Index j = 0; j < cell_elems; ++j)
                            out[out_row * cell_elems + j] = src[j];
                    }
                    else
                    {
                        for (Index j = 0; j < cell_elems; ++j)
                        {
                            Index sj = shape_plan.MapFlatIndex(j, static_cast<int>(k));
                            out[out_row * cell_elems + j] = src[sj];
                        }
                    }
                }
            }

            return Value(DataArray::CreateIndependent(std::move(*out_ds)));
        }

        // -- String path for Sweep ---

        static Value ExecSweepString(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            std::vector<DataShape> op_shapes;
            std::vector<Index> row_counts;
            for (size_t i = 0; i < ops.size(); ++i)
            {
                op_shapes.push_back(ops[i].data_shape());
                row_counts.push_back(ops[i].rows());
            }

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            Index cell_elems = shape_plan.result_elements;
            Index result_rows = info.rows;
            Index total = result_rows * cell_elems;

            std::vector<std::string> flat(static_cast<std::size_t>(total));

            Index out_row = 0;
            for (size_t k = 0; k < ops.size(); ++k)
            {
                Index n_rows = ops[k].rows();
                for (Index local_r = 0; local_r < n_rows; ++local_r, ++out_row)
                {
                    Index op_row = row_plan.broadcast[k] ? 0 : local_r;
                    Index base = out_row * cell_elems;

                    if (ops[k].is_measurement())
                    {
                        const Measurement& m = ops[k].as_measurement();
                        DataKind dk = m.data_kind();
                        if (dk == DataKind::kScalar)
                        {
                            std::string s = m.as_scalar<std::string>();
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = s;
                        }
                        else if (dk == DataKind::kVector)
                        {
                            auto vec = m.as_vector<std::string>();
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] =
                                    vec(shape_plan.MapFlatIndex(j, static_cast<int>(k)));
                        }
                        else
                        {
                            auto mat = m.as_matrix<std::string>();
                            Index cols = m.shape()[1];
                            for (Index j = 0; j < cell_elems; ++j)
                            {
                                Index sj = shape_plan.MapFlatIndex(j, static_cast<int>(k));
                                flat[static_cast<std::size_t>(base + j)] =
                                    mat(sj / cols, sj % cols);
                            }
                        }
                    }
                    else
                    {
                        const DataSeries& ds = ops[k].as_data_array().data();
                        DataKind dk = ds.data_kind();
                        if (dk == DataKind::kScalar)
                        {
                            std::string s = ds.scalar_at<std::string>(op_row);
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] = s;
                        }
                        else if (dk == DataKind::kVector)
                        {
                            auto vec = ds.vector_at<std::string>(op_row);
                            for (Index j = 0; j < cell_elems; ++j)
                                flat[static_cast<std::size_t>(base + j)] =
                                    vec(shape_plan.MapFlatIndex(j, static_cast<int>(k)));
                        }
                        else
                        {
                            auto mat = ds.matrix_at<std::string>(op_row);
                            Index cols = ds.data_shape()[1];
                            for (Index j = 0; j < cell_elems; ++j)
                            {
                                Index sj = shape_plan.MapFlatIndex(j, static_cast<int>(k));
                                flat[static_cast<std::size_t>(base + j)] =
                                    mat(sj / cols, sj % cols);
                            }
                        }
                    }
                }
            }

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kString, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(result_rows));
            for (Index r = 0; r < result_rows; ++r)
            {
                Index base = r * cell_elems;
                if (info.shape.kind() == DataKind::kScalar)
                    out_ds->scalar_at<std::string>(r) =
                        std::move(flat[static_cast<std::size_t>(base)]);
                else if (info.shape.kind() == DataKind::kVector)
                    for (Index j = 0; j < info.shape[0]; ++j)
                        out_ds->vector_at<std::string>(r)(j) =
                            std::move(flat[static_cast<std::size_t>(base + j)]);
                else
                    for (Index i = 0; i < info.shape[0]; ++i)
                        for (Index j = 0; j < info.shape[1]; ++j)
                            out_ds->matrix_at<std::string>(r)(i, j) = std::move(
                                flat[static_cast<std::size_t>(base + i * info.shape[1] + j)]);
            }
            return Value(DataArray::CreateIndependent(std::move(*out_ds)));
        }

        Value ExecuteSweep(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            if (ops.empty())
                throw std::invalid_argument("empty input");

            if (info.dtype == DataType::kString)
                return ExecSweepString(info, ops);

            switch (info.dtype)
            {
                case DataType::kComplex: return ExecSweepT<std::complex<double>>(info, ops);
                case DataType::kReal: return ExecSweepT<double>(info, ops);
                case DataType::kInteger: return ExecSweepT<int>(info, ops);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecuteUnaryNegate -- unary minus
        // =========================================================================

        Value ExecuteUnaryNegate(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecUnaryT<std::complex<double>>(info, ops, op_negate);
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_negate);
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, op_negate);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecuteUnaryNot -- logical NOT (!/NOT)
        // =========================================================================

        Value ExecuteUnaryNot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // Logical NOT: first convert to int via as_logical() (non-zero->),
            // then apply NOT.  Scalar Meas ->upgrade to Boolean.

            Value v;
            if (ops[0].is_measurement())
            {
                const Measurement& m = ops[0].as_measurement();
                DataSeries ds(m.data_type(), m.shape());
                ds.append(m);
                auto logical_ds = std::unique_ptr<DataSeries>(new DataSeries(ds.as_logical()));
                v = Value(logical_ds->measurement_at(0));
            }
            else
            {
                auto logical_ds = std::unique_ptr<DataSeries>(
                    new DataSeries(ops[0].as_data_array().data().as_logical()));
                auto da = std::make_shared<DataArray>(ops[0].as_data_array().clone());
                da->set_data(std::move(*logical_ds));
                v = Value(da);
            }

            Value result = ExecUnaryT<int>(info, {v}, op_not<int>);

            if (ops[0].is_measurement() && info.shape.kind() == DataKind::kScalar)
            {
                return Value::Boolean(result.as_measurement().as_scalar<int>() != 0);
            }
            return result;
        }

        // =========================================================================
        //  ExecuteUnaryBitNot -- bitwise NOT (~)
        // =========================================================================

        Value ExecuteUnaryBitNot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryT<int>(info, ops, op_bitnot<int>);
        }

        // =========================================================================
        //  ComputeInverse -- Eigen inverse, throws if singular
        // =========================================================================

        template <typename T>
        static Mat<T> ComputeInverse(const T* B, Index n)
        {
            MatConstMap<T> Bmap(B, n, n);
            Eigen::FullPivLU<Mat<T>> lu(Bmap);

            if (!lu.isInvertible())
                throw std::invalid_argument("RHS matrix is singular; division undefined");

            return lu.inverse();
        }

        // =========================================================================
        //  ExecBinaryMatMulT -- matrix multiplication with row broadcast
        // =========================================================================

        template <typename T>
        Value ExecBinaryMatMulT(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            bool l_meas = ops[0].is_measurement();
            bool r_meas = ops[1].is_measurement();

            auto rcA = EffectiveRC(ops[0].data_shape());
            auto rcB = EffectiveRC(ops[1].data_shape());
            Index rA = rcA.first, cA = rcA.second;
            Index rB = rcB.first, cB = rcB.second;
            Index rC = rA, cC = cB;

            Index l_rows = ops[0].rows();
            Index r_rows = ops[1].rows();
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute({l_rows, r_rows});

            auto l_in = ops[0].flat_data<T>();
            auto r_in = ops[1].flat_data<T>();
            const T* l_ptr = l_in.ptr;
            const T* r_ptr = r_in.ptr;
            Index l_stride = l_in.stride;
            Index r_stride = r_in.stride;

            const DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index out_stride = rC * cC;

            for (Index i = 0; i < info.rows; ++i)
            {
                Index l_off = (row_plan.broadcast[0] ? 0 : i) * l_stride;
                Index r_off = (row_plan.broadcast[1] ? 0 : i) * r_stride;

                MatConstMap<T> A(l_ptr + l_off, rA, cA);
                MatConstMap<T> B(r_ptr + r_off, rB, cB);
                MatMap<T> C(out + i * out_stride, rC, cC);
                C.noalias() = A * B;
            }

            if (l_meas && r_meas)
            {
                return Value(out_ds->measurement_at(0));
            }
            else
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // =========================================================================
        //  ExecBinaryDivT -- matrix division kernel (A x inv(B)) with row broadcast
        // =========================================================================

        template <typename T>
        Value ExecBinaryDivT(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            bool l_meas = ops[0].is_measurement();
            bool r_meas = ops[1].is_measurement();

            auto rcA = EffectiveRC(ops[0].data_shape());
            auto rcB = EffectiveRC(ops[1].data_shape());
            Index rA = rcA.first, cA = rcA.second;
            Index rB = rcB.first, cB = rcB.second;
            // inv(B): effective shape (cB, rB), so A x inv(B) -> (rA, rB)
            Index rC = rA, cC = rB;

            Index l_rows = ops[0].rows();
            Index r_rows = ops[1].rows();
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute({l_rows, r_rows});

            auto l_in = ops[0].flat_data<T>();
            auto r_in = ops[1].flat_data<T>();
            const T* l_ptr = l_in.ptr;
            const T* r_ptr = r_in.ptr;
            Index l_stride = l_in.stride;
            Index r_stride = r_in.stride;

            const DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index out_stride = rC * cC;

            for (Index i = 0; i < info.rows; ++i)
            {
                Index l_off = (row_plan.broadcast[0] ? 0 : i) * l_stride;
                Index r_off = (row_plan.broadcast[1] ? 0 : i) * r_stride;

                // Compute inverse of B, then A x inv(B)
                Mat<T> invB = ComputeInverse(r_ptr + r_off, rB);

                MatConstMap<T> A(l_ptr + l_off, rA, cA);
                MatMap<T> C(out + i * out_stride, rC, cC);
                C.noalias() = A * invB;
            }

            if (l_meas && r_meas)
            {
                return Value(out_ds->measurement_at(0));
            }
            else
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // =========================================================================
        //  ExecuteBinaryMul -- dispatches scalar (element-wise) vs matrix multiply
        // =========================================================================

        Value ExecuteBinaryMul(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // Scalar path: either operand is scalar -> element-wise broadcast
            if (ops[0].data_shape().kind() == DataKind::kScalar ||
                ops[1].data_shape().kind() == DataKind::kScalar)
            {
                switch (info.dtype)
                {
                    case DataType::kComplex:
                        return ExecBinaryArithT<std::complex<double>>(
                            info, ops, op_mul<std::complex<double>>);
                    case DataType::kReal:
                        return ExecBinaryArithT<double>(info, ops, op_mul<double>);
                    case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_mul<int>);
                    default: throw std::invalid_argument("unsupported dtype");
                }
            }

            // Matrix multiplication path
            switch (info.dtype)
            {
                case DataType::kComplex: return ExecBinaryMatMulT<std::complex<double>>(info, ops);
                case DataType::kReal: return ExecBinaryMatMulT<double>(info, ops);
                case DataType::kInteger: return ExecBinaryMatMulT<int>(info, ops);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecuteBinaryDiv -- dispatches scalar (element-wise) vs A x inv(B)
        // =========================================================================

        Value ExecuteBinaryDiv(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // Scalar path: RHS is scalar -> element-wise broadcast
            if (ops[1].data_shape().kind() == DataKind::kScalar)
            {
                // Zero-divisor check for scalar Measurement / Measurement
                if (ops[0].is_measurement() && ops[1].is_measurement() &&
                    info.shape.kind() == DataKind::kScalar)
                {
                    switch (info.dtype)
                    {
                        case DataType::kComplex:
                        {
                            auto r_in = ops[1].flat_data<std::complex<double>>();
                            if (r_in.ptr[0] == std::complex<double>(0))
                                throw std::invalid_argument("division by zero");
                            break;
                        }
                        case DataType::kReal:
                        {
                            auto r_in = ops[1].flat_data<double>();
                            if (r_in.ptr[0] == 0.0)
                                throw std::invalid_argument("division by zero");
                            break;
                        }
                        case DataType::kInteger:
                        {
                            auto r_in = ops[1].flat_data<int>();
                            if (r_in.ptr[0] == 0)
                                throw std::invalid_argument("division by zero");
                            break;
                        }
                        default: break;
                    }
                }
                switch (info.dtype)
                {
                    case DataType::kComplex:
                        return ExecBinaryArithT<std::complex<double>>(
                            info, ops, op_div<std::complex<double>>);
                    case DataType::kReal:
                        return ExecBinaryArithT<double>(info, ops, op_div<double>);
                    case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_div<int>);
                    default: throw std::invalid_argument("unsupported dtype");
                }
            }

            // Matrix division path: A x inv(B)
            // (dtype is always >=Real; DeriveDtypeDiv promotes int->real)
            switch (info.dtype)
            {
                case DataType::kComplex: return ExecBinaryDivT<std::complex<double>>(info, ops);
                case DataType::kReal: return ExecBinaryDivT<double>(info, ops);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  ExecuteConditional -- ternary conditional (condition ? true_val : false_val)
        // =========================================================================
        //
        //  Converts condition to logical int (0/1) via as_logical(), then for each
        //  element picks from true_val (condition != 0) or false_val (condition == 0).
        //  Supports both row broadcast and shape broadcast across all three operands.

        template <typename T>
        static Value ExecConditionalT(const ExecContextInfo& info,
                                      const std::vector<Value>& ops,
                                      const Value& cond,
                                      const DataArray* out_src,
                                      bool c_meas,
                                      bool t_meas,
                                      bool f_meas)
        {
            DataShape c_shape = cond.data_shape();
            DataShape t_shape = ops[1].data_shape();
            DataShape f_shape = ops[2].data_shape();
            std::vector<DataShape> op_shapes = {c_shape, t_shape, f_shape};

            Index c_rows = cond.rows();
            Index t_rows = ops[1].rows();
            Index f_rows = ops[2].rows();
            std::vector<Index> row_counts = {c_rows, t_rows, f_rows};

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            auto c_in = cond.flat_data<int>();
            auto t_in = ops[1].flat_data<T>();
            auto f_in = ops[2].flat_data<T>();

            const int* c_ptr = c_in.ptr;
            Index c_stride = c_in.stride;
            const T* t_ptr = t_in.ptr;
            Index t_stride = t_in.stride;
            const T* f_ptr = f_in.ptr;
            Index f_stride = f_in.stride;

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index c_off = (row_plan.broadcast[0] ? 0 : i) * c_stride;
                Index t_off = (row_plan.broadcast[1] ? 0 : i) * t_stride;
                Index f_off = (row_plan.broadcast[2] ? 0 : i) * f_stride;
                Index o_off = i * out_stride;

                for (Index j = 0; j < shape_plan.result_elements; ++j)
                {
                    Index cj = shape_plan.MapFlatIndex(j, 0);
                    Index tj = shape_plan.MapFlatIndex(j, 1);
                    Index fj = shape_plan.MapFlatIndex(j, 2);
                    out[o_off + j] =
                        (c_ptr[c_off + cj] != 0) ? t_ptr[t_off + tj] : f_ptr[f_off + fj];
                }
            }

            if (c_meas && t_meas && f_meas)
                return Value(out_ds->measurement_at(0));
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // -- String path: read strings directly, no flat_data ---

        static Value ExecConditionalString(const ExecContextInfo& info,
                                           const std::vector<Value>& ops,
                                           const Value& cond,
                                           const DataArray* out_src,
                                           bool c_meas,
                                           bool t_meas,
                                           bool f_meas)
        {
            DataShape c_shape = cond.data_shape();
            DataShape t_shape = ops[1].data_shape();
            DataShape f_shape = ops[2].data_shape();
            std::vector<DataShape> op_shapes = {c_shape, t_shape, f_shape};

            Index c_rows = cond.rows();
            Index t_rows = ops[1].rows();
            Index f_rows = ops[2].rows();
            std::vector<Index> row_counts = {c_rows, t_rows, f_rows};

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            auto c_in = cond.flat_data<int>();
            const int* c_ptr = c_in.ptr;
            Index c_stride = c_in.stride;

            Index t_stride = static_cast<Index>(t_shape.element_count());
            Index f_stride = static_cast<Index>(f_shape.element_count());

            std::vector<std::string> t_flat, f_flat;
            auto read_flat =
                [](const Value& v, Index rows, Index stride, std::vector<std::string>& out)
            {
                out.resize(static_cast<std::size_t>(rows * stride));
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataKind dk = m.data_kind();
                    if (dk == DataKind::kScalar)
                    {
                        std::string s = m.as_scalar<std::string>();
                        for (Index i = 0; i < rows * stride; ++i)
                            out[static_cast<std::size_t>(i)] = s;
                    }
                    else if (dk == DataKind::kVector)
                    {
                        auto vec = m.as_vector<std::string>();
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] = vec(j);
                    }
                    else
                    {
                        auto mat = m.as_matrix<std::string>();
                        Index cols = m.shape()[1];
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    mat(j / cols, j % cols);
                    }
                }
                else
                {
                    const DataSeries& ds = v.as_data_array().data();
                    DataKind dk = ds.data_kind();
                    for (Index i = 0; i < rows; ++i)
                    {
                        Index src_row = i;
                        if (dk == DataKind::kScalar)
                            out[static_cast<std::size_t>(i * stride)] =
                                ds.scalar_at<std::string>(src_row);
                        else if (dk == DataKind::kVector)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.vector_at<std::string>(src_row)(j);
                        else
                        {
                            Index cols = ds.data_shape()[1];
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.matrix_at<std::string>(src_row)(j / cols, j % cols);
                        }
                    }
                }
            };
            read_flat(ops[1], t_rows, t_stride, t_flat);
            read_flat(ops[2], f_rows, f_stride, f_flat);

            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kString, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));

            Index out_stride = shape_plan.result_elements;
            Index result_rows_total = info.rows * out_stride;

            if (c_meas && t_meas && f_meas)
            {
                // Measurement output ->use string tensors or scalar directly
                DataKind dk = info.shape.kind();
                if (dk == DataKind::kScalar)
                {
                    Index cj = shape_plan.MapFlatIndex(0, 0);
                    Index tj = shape_plan.MapFlatIndex(0, 1);
                    Index fj = shape_plan.MapFlatIndex(0, 2);
                    std::string val = (c_ptr[cj] != 0) ? t_flat[static_cast<std::size_t>(tj)]
                                                       : f_flat[static_cast<std::size_t>(fj)];
                    return Value::String(std::move(val));
                }
                if (dk == DataKind::kVector)
                {
                    Index w = info.shape[0];
                    VecXs vec(w);
                    for (Index i = 0; i < info.rows; ++i)
                    {
                        Index c_off = (row_plan.broadcast[0] ? 0 : i) * c_stride;
                        Index t_off = (row_plan.broadcast[1] ? 0 : i) * t_stride;
                        Index f_off = (row_plan.broadcast[2] ? 0 : i) * f_stride;

                        for (Index j = 0; j < out_stride; ++j)
                        {
                            Index cj = shape_plan.MapFlatIndex(j, 0);
                            Index tj = shape_plan.MapFlatIndex(j, 1);
                            Index fj = shape_plan.MapFlatIndex(j, 2);
                            Index idx = static_cast<Index>(
                                static_cast<std::size_t>(i) * static_cast<std::size_t>(out_stride) +
                                static_cast<std::size_t>(j));
                            if (c_ptr[c_off + cj] != 0)
                                vec(idx % w) = t_flat[static_cast<std::size_t>(t_off + tj)];
                            else
                                vec(idx % w) = f_flat[static_cast<std::size_t>(f_off + fj)];
                        }
                    }
                    return Value::Vector(vec);
                }
                else
                {
                    Index rows = info.shape[0], cols = info.shape[1];
                    MatXs mat(rows, cols);
                    for (Index i = 0; i < info.rows; ++i)
                    {
                        Index c_off = (row_plan.broadcast[0] ? 0 : i) * c_stride;
                        Index t_off = (row_plan.broadcast[1] ? 0 : i) * t_stride;
                        Index f_off = (row_plan.broadcast[2] ? 0 : i) * f_stride;

                        for (Index j = 0; j < out_stride; ++j)
                        {
                            Index cj = shape_plan.MapFlatIndex(j, 0);
                            Index tj = shape_plan.MapFlatIndex(j, 1);
                            Index fj = shape_plan.MapFlatIndex(j, 2);
                            if (c_ptr[c_off + cj] != 0)
                                mat(j / cols, j % cols) =
                                    t_flat[static_cast<std::size_t>(t_off + tj)];
                            else
                                mat(j / cols, j % cols) =
                                    f_flat[static_cast<std::size_t>(f_off + fj)];
                        }
                    }
                    return Value::Matrix(mat);
                }
            }

            // DataArray output
            for (Index i = 0; i < info.rows; ++i)
            {
                Index c_off = (row_plan.broadcast[0] ? 0 : i) * c_stride;
                Index t_off = (row_plan.broadcast[1] ? 0 : i) * t_stride;
                Index f_off = (row_plan.broadcast[2] ? 0 : i) * f_stride;

                if (info.shape.kind() == DataKind::kScalar)
                {
                    Index cj = shape_plan.MapFlatIndex(0, 0);
                    Index tj = shape_plan.MapFlatIndex(0, 1);
                    Index fj = shape_plan.MapFlatIndex(0, 2);
                    out_ds->scalar_at<std::string>(i) =
                        (c_ptr[c_off + cj] != 0) ? t_flat[static_cast<std::size_t>(t_off + tj)]
                                                 : f_flat[static_cast<std::size_t>(f_off + fj)];
                }
                else if (info.shape.kind() == DataKind::kVector)
                {
                    for (Index j = 0; j < out_stride; ++j)
                    {
                        Index cj = shape_plan.MapFlatIndex(j, 0);
                        Index tj = shape_plan.MapFlatIndex(j, 1);
                        Index fj = shape_plan.MapFlatIndex(j, 2);
                        out_ds->vector_at<std::string>(i)(j) =
                            (c_ptr[c_off + cj] != 0) ? t_flat[static_cast<std::size_t>(t_off + tj)]
                                                     : f_flat[static_cast<std::size_t>(f_off + fj)];
                    }
                }
                else
                {
                    for (Index j = 0; j < out_stride; ++j)
                    {
                        Index cj = shape_plan.MapFlatIndex(j, 0);
                        Index tj = shape_plan.MapFlatIndex(j, 1);
                        Index fj = shape_plan.MapFlatIndex(j, 2);
                        Index row = j / info.shape[1];
                        Index col = j % info.shape[1];
                        out_ds->matrix_at<std::string>(i)(row, col) =
                            (c_ptr[c_off + cj] != 0) ? t_flat[static_cast<std::size_t>(t_off + tj)]
                                                     : f_flat[static_cast<std::size_t>(f_off + fj)];
                    }
                }
            }
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        Value ExecuteConditional(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // ops: [condition, true_value, false_value]

            // ---- convert condition to logical int (0/1) ----
            auto make_logical = [](const Value& v) -> Value
            {
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataSeries ds(m.data_type(), m.shape());
                    ds.append(m);
                    auto logical_ds = std::unique_ptr<DataSeries>(new DataSeries(ds.as_logical()));
                    return Value(logical_ds->measurement_at(0));
                }
                else
                {
                    auto logical_ds = std::unique_ptr<DataSeries>(
                        new DataSeries(v.as_data_array().data().as_logical()));
                    auto da = std::make_shared<DataArray>(v.as_data_array().clone());
                    da->set_data(std::move(*logical_ds));
                    return Value(da);
                }
            };

            Value cond = make_logical(ops[0]);

            bool t_meas = ops[1].is_measurement();
            bool f_meas = ops[2].is_measurement();
            bool c_meas = cond.is_measurement();

            // Output source: inherit metadata from true/false DataArray
            const DataArray* out_src = nullptr;
            if (!t_meas && !f_meas)
                out_src = &ops[1].as_data_array();
            else if (!t_meas)
                out_src = &ops[1].as_data_array();
            else if (!f_meas)
                out_src = &ops[2].as_data_array();

            if (info.dtype == DataType::kString)
                return ExecConditionalString(info, ops, cond, out_src, c_meas, t_meas, f_meas);

            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecConditionalT<std::complex<double>>(
                        info, ops, cond, out_src, c_meas, t_meas, f_meas);
                case DataType::kReal:
                    return ExecConditionalT<double>(
                        info, ops, cond, out_src, c_meas, t_meas, f_meas);
                default:
                    return ExecConditionalT<int>(info, ops, cond, out_src, c_meas, t_meas, f_meas);
            }
        }

        // =========================================================================
        //  ExecuteIf -- multi-branch if/elseif/else
        // =========================================================================
        //
        //  operands: [cond0, val0, cond1, val1, ..., cond_{n-1}, val_{n-1}, else]
        //  size = 2n+1, n >= 1.
        //
        //  Converts each condition to logical int (0/1) via as_logical(), then for
        //  each element picks the value from the first branch whose condition is
        //  non-zero, falling back to the final else operand.

        template <typename T>
        static Value ExecIfT(const ExecContextInfo& info,
                             const std::vector<Value>& ops,
                             const DataArray* out_src)
        {
            Index n = static_cast<Index>(ops.size());
            Index num_pairs = (n - 1) / 2;

            // --- collect shapes & row counts for all operands ---
            std::vector<DataShape> op_shapes;
            std::vector<Index> row_counts;
            for (Index i = 0; i < n; ++i)
            {
                op_shapes.push_back(ops[i].data_shape());
                row_counts.push_back(ops[i].rows());
            }

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            // --- acquire flat inputs: conditions (int), values (T) ---
            std::vector<Value::FlatData<int>> cond_ins(num_pairs);
            std::vector<Value::FlatData<T>> val_ins(num_pairs);
            for (Index p = 0; p < num_pairs; ++p)
            {
                cond_ins[p] = ops[2 * p].flat_data<int>();
                val_ins[p] = ops[2 * p + 1].flat_data<T>();
            }
            auto else_in = ops[n - 1].flat_data<T>();

            // --- allocate output ---
            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataTypeOf<T>::tag, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            T* out = out_ds->mutable_contiguous_data<T>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index o_off = i * out_stride;

                // Per-row offsets for conditions & values
                std::vector<Index> c_off(num_pairs), v_off(num_pairs);
                for (Index p = 0; p < num_pairs; ++p)
                {
                    c_off[p] = (row_plan.broadcast[2 * p] ? 0 : i) * cond_ins[p].stride;
                    v_off[p] = (row_plan.broadcast[2 * p + 1] ? 0 : i) * val_ins[p].stride;
                }
                Index e_off = (row_plan.broadcast[n - 1] ? 0 : i) * else_in.stride;

                for (Index j = 0; j < shape_plan.result_elements; ++j)
                {
                    // Map flat index j to per-operand indices
                    std::vector<Index> cj(num_pairs), vj(num_pairs);
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        cj[p] = shape_plan.MapFlatIndex(j, 2 * p);
                        vj[p] = shape_plan.MapFlatIndex(j, 2 * p + 1);
                    }
                    Index ej = shape_plan.MapFlatIndex(j, n - 1);

                    // Evaluate branches in order
                    bool matched = false;
                    T result_val{};
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        if (cond_ins[p].ptr[c_off[p] + cj[p]] != 0)
                        {
                            result_val = val_ins[p].ptr[v_off[p] + vj[p]];
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                        result_val = else_in.ptr[e_off + ej];

                    out[o_off + j] = result_val;
                }
            }

            // --- determine if all operands are measurements ---
            bool all_meas = true;
            for (Index i = 0; i < n; ++i)
            {
                if (!ops[i].is_measurement())
                {
                    all_meas = false;
                    break;
                }
            }
            if (all_meas)
                return Value(out_ds->measurement_at(0));
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        // -- String path for If ---

        static Value ExecIfString(const ExecContextInfo& info,
                                  const std::vector<Value>& ops,
                                  const DataArray* out_src)
        {
            Index n = static_cast<Index>(ops.size());
            Index num_pairs = (n - 1) / 2;

            // --- collect shapes & row counts ---
            std::vector<DataShape> op_shapes;
            std::vector<Index> row_counts;
            for (Index i = 0; i < n; ++i)
            {
                op_shapes.push_back(ops[i].data_shape());
                row_counts.push_back(ops[i].rows());
            }

            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
            RowBroadcastPlan row_plan = RowBroadcastPlan::Compute(row_counts);

            // --- acquire flat condition inputs (int) ---
            std::vector<Value::FlatData<int>> cond_ins(num_pairs);
            for (Index p = 0; p < num_pairs; ++p)
                cond_ins[p] = ops[2 * p].flat_data<int>();

            // --- read all value operands as flat string vectors ---
            auto read_flat =
                [](const Value& v, Index rows, Index stride, std::vector<std::string>& out)
            {
                out.resize(static_cast<std::size_t>(rows * stride));
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataKind dk = m.data_kind();
                    if (dk == DataKind::kScalar)
                    {
                        std::string s = m.as_scalar<std::string>();
                        for (Index i = 0; i < rows * stride; ++i)
                            out[static_cast<std::size_t>(i)] = s;
                    }
                    else if (dk == DataKind::kVector)
                    {
                        auto vec = m.as_vector<std::string>();
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] = vec(j);
                    }
                    else
                    {
                        auto mat = m.as_matrix<std::string>();
                        Index cols = m.shape()[1];
                        for (Index i = 0; i < rows; ++i)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    mat(j / cols, j % cols);
                    }
                }
                else
                {
                    const DataSeries& ds = v.as_data_array().data();
                    DataKind dk = ds.data_kind();
                    for (Index i = 0; i < rows; ++i)
                    {
                        Index src_row = i;
                        if (dk == DataKind::kScalar)
                            out[static_cast<std::size_t>(i * stride)] =
                                ds.scalar_at<std::string>(src_row);
                        else if (dk == DataKind::kVector)
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.vector_at<std::string>(src_row)(j);
                        else
                        {
                            Index cols = ds.data_shape()[1];
                            for (Index j = 0; j < stride; ++j)
                                out[static_cast<std::size_t>(i * stride + j)] =
                                    ds.matrix_at<std::string>(src_row)(j / cols, j % cols);
                        }
                    }
                }
            };

            std::vector<std::vector<std::string>> val_flats(num_pairs);
            std::vector<Index> val_strides(num_pairs);
            for (Index p = 0; p < num_pairs; ++p)
            {
                Index idx = 2 * p + 1;
                val_strides[p] = static_cast<Index>(op_shapes[idx].element_count());
                read_flat(ops[idx], row_counts[idx], val_strides[p], val_flats[p]);
            }
            Index else_stride = static_cast<Index>(op_shapes[n - 1].element_count());
            std::vector<std::string> else_flat;
            read_flat(ops[n - 1], row_counts[n - 1], else_stride, else_flat);

            // --- check if all operands are measurements ---
            bool all_meas = true;
            for (Index i = 0; i < n; ++i)
            {
                if (!ops[i].is_measurement())
                {
                    all_meas = false;
                    break;
                }
            }

            Index out_stride = shape_plan.result_elements;
            Index result_rows_total = info.rows * out_stride;

            if (all_meas)
            {
                DataKind dk = info.shape.kind();
                if (dk == DataKind::kScalar)
                {
                    std::vector<Index> cj0(num_pairs), vj0(num_pairs);
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        cj0[p] = shape_plan.MapFlatIndex(0, 2 * p);
                        vj0[p] = shape_plan.MapFlatIndex(0, 2 * p + 1);
                    }
                    Index ej0 = shape_plan.MapFlatIndex(0, n - 1);
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        if (cond_ins[p].ptr[cj0[p]] != 0)
                            return Value::String(val_flats[p][static_cast<std::size_t>(vj0[p])]);
                    }
                    return Value::String(else_flat[static_cast<std::size_t>(ej0)]);
                }
                if (dk == DataKind::kVector)
                {
                    Index w = info.shape[0];
                    VecXs vec(w * info.rows);
                    for (Index i = 0; i < info.rows; ++i)
                    {
                        std::vector<Index> c_off(num_pairs), v_off(num_pairs);
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            c_off[p] = (row_plan.broadcast[2 * p] ? 0 : i) * cond_ins[p].stride;
                            v_off[p] = (row_plan.broadcast[2 * p + 1] ? 0 : i) * val_strides[p];
                        }
                        Index e_off = (row_plan.broadcast[n - 1] ? 0 : i) * else_stride;

                        for (Index j = 0; j < out_stride; ++j)
                        {
                            std::vector<Index> cj(num_pairs), vj(num_pairs);
                            for (Index p = 0; p < num_pairs; ++p)
                            {
                                cj[p] = shape_plan.MapFlatIndex(j, 2 * p);
                                vj[p] = shape_plan.MapFlatIndex(j, 2 * p + 1);
                            }
                            Index ej = shape_plan.MapFlatIndex(j, n - 1);
                            Index idx = i * out_stride + j;

                            bool matched = false;
                            for (Index p = 0; p < num_pairs; ++p)
                            {
                                if (cond_ins[p].ptr[c_off[p] + cj[p]] != 0)
                                {
                                    vec(idx % w) =
                                        val_flats[p][static_cast<std::size_t>(v_off[p] + vj[p])];
                                    matched = true;
                                    break;
                                }
                            }
                            if (!matched)
                                vec(idx % w) = else_flat[static_cast<std::size_t>(e_off + ej)];
                        }
                    }
                    return Value::Vector(vec);
                }
                else
                {
                    // Matrix Measurement
                    Index rows = info.shape[0], cols = info.shape[1];
                    MatXs mat(rows, cols);
                    for (Index i = 0; i < info.rows; ++i)
                    {
                        std::vector<Index> c_off(num_pairs), v_off(num_pairs);
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            c_off[p] = (row_plan.broadcast[2 * p] ? 0 : i) * cond_ins[p].stride;
                            v_off[p] = (row_plan.broadcast[2 * p + 1] ? 0 : i) * val_strides[p];
                        }
                        Index e_off = (row_plan.broadcast[n - 1] ? 0 : i) * else_stride;

                        for (Index j = 0; j < out_stride; ++j)
                        {
                            std::vector<Index> cj(num_pairs), vj(num_pairs);
                            for (Index p = 0; p < num_pairs; ++p)
                            {
                                cj[p] = shape_plan.MapFlatIndex(j, 2 * p);
                                vj[p] = shape_plan.MapFlatIndex(j, 2 * p + 1);
                            }
                            Index ej = shape_plan.MapFlatIndex(j, n - 1);

                            bool matched = false;
                            for (Index p = 0; p < num_pairs; ++p)
                            {
                                if (cond_ins[p].ptr[c_off[p] + cj[p]] != 0)
                                {
                                    mat(j / cols, j % cols) =
                                        val_flats[p][static_cast<std::size_t>(v_off[p] + vj[p])];
                                    matched = true;
                                    break;
                                }
                            }
                            if (!matched)
                                mat(j / cols, j % cols) =
                                    else_flat[static_cast<std::size_t>(e_off + ej)];
                        }
                    }
                    return Value::Matrix(mat);
                }
            }

            // DataArray output
            auto out_ds =
                std::unique_ptr<DataSeries>(new DataSeries(DataType::kString, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));

            for (Index i = 0; i < info.rows; ++i)
            {
                std::vector<Index> c_off(num_pairs), v_off(num_pairs);
                for (Index p = 0; p < num_pairs; ++p)
                {
                    c_off[p] = (row_plan.broadcast[2 * p] ? 0 : i) * cond_ins[p].stride;
                    v_off[p] = (row_plan.broadcast[2 * p + 1] ? 0 : i) * val_strides[p];
                }
                Index e_off = (row_plan.broadcast[n - 1] ? 0 : i) * else_stride;

                if (info.shape.kind() == DataKind::kScalar)
                {
                    std::vector<Index> cj0(num_pairs), vj0(num_pairs);
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        cj0[p] = shape_plan.MapFlatIndex(0, 2 * p);
                        vj0[p] = shape_plan.MapFlatIndex(0, 2 * p + 1);
                    }
                    Index ej0 = shape_plan.MapFlatIndex(0, n - 1);
                    bool matched = false;
                    for (Index p = 0; p < num_pairs; ++p)
                    {
                        if (cond_ins[p].ptr[c_off[p] + cj0[p]] != 0)
                        {
                            out_ds->scalar_at<std::string>(i) =
                                val_flats[p][static_cast<std::size_t>(v_off[p] + vj0[p])];
                            matched = true;
                            break;
                        }
                    }
                    if (!matched)
                        out_ds->scalar_at<std::string>(i) =
                            else_flat[static_cast<std::size_t>(e_off + ej0)];
                }
                else if (info.shape.kind() == DataKind::kVector)
                {
                    for (Index j = 0; j < out_stride; ++j)
                    {
                        std::vector<Index> cj(num_pairs), vj(num_pairs);
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            cj[p] = shape_plan.MapFlatIndex(j, 2 * p);
                            vj[p] = shape_plan.MapFlatIndex(j, 2 * p + 1);
                        }
                        Index ej = shape_plan.MapFlatIndex(j, n - 1);
                        bool matched = false;
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            if (cond_ins[p].ptr[c_off[p] + cj[p]] != 0)
                            {
                                out_ds->vector_at<std::string>(i)(j) =
                                    val_flats[p][static_cast<std::size_t>(v_off[p] + vj[p])];
                                matched = true;
                                break;
                            }
                        }
                        if (!matched)
                            out_ds->vector_at<std::string>(i)(j) =
                                else_flat[static_cast<std::size_t>(e_off + ej)];
                    }
                }
                else
                {
                    for (Index j = 0; j < out_stride; ++j)
                    {
                        std::vector<Index> cj(num_pairs), vj(num_pairs);
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            cj[p] = shape_plan.MapFlatIndex(j, 2 * p);
                            vj[p] = shape_plan.MapFlatIndex(j, 2 * p + 1);
                        }
                        Index ej = shape_plan.MapFlatIndex(j, n - 1);
                        Index row = j / info.shape[1];
                        Index col = j % info.shape[1];
                        bool matched = false;
                        for (Index p = 0; p < num_pairs; ++p)
                        {
                            if (cond_ins[p].ptr[c_off[p] + cj[p]] != 0)
                            {
                                out_ds->matrix_at<std::string>(i)(row, col) =
                                    val_flats[p][static_cast<std::size_t>(v_off[p] + vj[p])];
                                matched = true;
                                break;
                            }
                        }
                        if (!matched)
                            out_ds->matrix_at<std::string>(i)(row, col) =
                                else_flat[static_cast<std::size_t>(e_off + ej)];
                    }
                }
            }
            {
                auto da = std::make_shared<DataArray>(out_src->clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }

        Value ExecuteIf(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            // ops: [cond0, val0, cond1, val1, ..., cond_{n-1}, val_{n-1}, else]
            Index n = static_cast<Index>(ops.size());

            // Validate: must be odd and at least 3
            if (n < 3 || n % 2 == 0)
                throw std::invalid_argument(
                    "requires odd number of operands >= 3 (2n+1), got " + std::to_string(n));

            // --- convert all conditions to logical int (0/1) ---
            auto make_logical = [](const Value& v) -> Value
            {
                if (v.is_measurement())
                {
                    const Measurement& m = v.as_measurement();
                    DataSeries ds(m.data_type(), m.shape());
                    ds.append(m);
                    auto logical_ds = std::unique_ptr<DataSeries>(new DataSeries(ds.as_logical()));
                    return Value(logical_ds->measurement_at(0));
                }
                else
                {
                    auto logical_ds = std::unique_ptr<DataSeries>(
                        new DataSeries(v.as_data_array().data().as_logical()));
                    auto da = std::make_shared<DataArray>(v.as_data_array().clone());
                    da->set_data(std::move(*logical_ds));
                    return Value(da);
                }
            };

            std::vector<Value> logical_ops = ops;
            Index num_pairs = (n - 1) / 2;
            for (Index p = 0; p < num_pairs; ++p)
                logical_ops[2 * p] = make_logical(ops[2 * p]);

            // Output source: inherit metadata from first non-measurement value operand
            const DataArray* out_src = nullptr;
            for (Index i = 1; i < n; ++i)
            {
                if ((i % 2 == 1 || i == n - 1) && !logical_ops[i].is_measurement())
                {
                    out_src = &logical_ops[i].as_data_array();
                    break;
                }
            }

            if (info.dtype == DataType::kString)
                return ExecIfString(info, logical_ops, out_src);

            switch (info.dtype)
            {
                case DataType::kComplex:
                    return ExecIfT<std::complex<double>>(info, logical_ops, out_src);
                case DataType::kReal: return ExecIfT<double>(info, logical_ops, out_src);
                default: return ExecIfT<int>(info, logical_ops, out_src);
            }
        }

        // =========================================================================
        //  Predefined OpTraits
        // =========================================================================

        // ---- binary arithmetic -----------------------------------------------------

        const OpTraits kOpAdd = {2,
                                 "add",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromote,
                                 DeriveUnitPromoteDimension,
                                 ExecuteAdd};

        const OpTraits kOpSub = {2,
                                 "subtract",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromote,
                                 DeriveUnitPromoteDimension,
                                 ExecuteSub};

        const OpTraits kOpMul = {2,
                                 "multiply",
                                 DeriveShapeMul,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromote,
                                 DeriveUnitMul,
                                 ExecuteBinaryMul};

        const OpTraits kOpDiv = {2,
                                 "divide",
                                 DeriveShapeDiv,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitDiv,
                                 ExecuteBinaryDiv};

        const OpTraits kOpTimes = {2,
                                   "times",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromote,
                                   DeriveUnitMul,
                                   ExecuteTimes};

        const OpTraits kOpRdivide = {2,
                                     "rdivide",
                                     DeriveShapeBroadcast,
                                     DeriveRowsBroadcast,
                                     DeriveDtypePromoteReal,
                                     DeriveUnitDiv,
                                     ExecuteRdivide};

        const OpTraits kOpMod = {2,
                                 "mod",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteNoComplex,
                                 DeriveUnitMod,
                                 ExecuteMod};

        const OpTraits kOpPow = {2,
                                 "pow",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitDimlessRight,
                                 ExecutePow};

        // ---- binary comparison -----------------------------------------------------

        const OpTraits kOpEq = {2,
                                 "equal",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypePromoteWithString,
                                DeriveUnitDimless,
                                ExecuteEq};

        const OpTraits kOpNeq = {2,
                                 "notequal",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteWithString,
                                 DeriveUnitDimless,
                                 ExecuteNeq};

        const OpTraits kOpLt = {2,
                                 "lessthan",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypePromoteWithString,
                                DeriveUnitDimless,
                                ExecuteLt};

        const OpTraits kOpGt = {2,
                                 "greaterthan",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypePromoteWithString,
                                DeriveUnitDimless,
                                ExecuteGt};

        const OpTraits kOpLe = {2,
                                 "lessequal",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypePromoteWithString,
                                DeriveUnitDimless,
                                ExecuteLe};

        const OpTraits kOpGe = {2,
                                 "greaterequal",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypePromoteWithString,
                                DeriveUnitDimless,
                                ExecuteGe};

        // ---- binary logical --------------------------------------------------------

        const OpTraits kOpAnd = {2,
                                 "and",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeAlwaysInt,
                                 DeriveUnitDimless,
                                 ExecuteAnd};

        const OpTraits kOpOr = {2,
                                 "or",
                                 DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypeAlwaysInt,
                                DeriveUnitDimless,
                                ExecuteOr};

        // ---- binary bitwise ----------------------------------------------------

        const OpTraits kOpBitAnd = {2,
                                 "bitand",
                                 DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypeRequireInt,
                                    DeriveUnitPromoteDimension,
                                    ExecuteBitAnd};

        const OpTraits kOpBitOr = {2,
                                 "bitor",
                                 DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeRequireInt,
                                   DeriveUnitPromoteDimension,
                                   ExecuteBitOr};

        const OpTraits kOpBitXor = {2,
                                 "bitxor",
                                 DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypeRequireInt,
                                    DeriveUnitPromoteDimension,
                                    ExecuteBitXor};

        // ---- binary shift ------------------------------------------------------

        const OpTraits kOpShl = {2,
                                 "shiftleft",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeRequireInt,
                                 DeriveUnitDimlessRight,
                                 ExecuteShl};

        const OpTraits kOpShr = {2,
                                 "shiftright",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeRequireInt,
                                 DeriveUnitDimlessRight,
                                 ExecuteShr};

        // ---- unary ----------------------------------------------------------------

        const OpTraits kOpNegate = {1,
                                 "negate",
                                 DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypePromote,
                                    DeriveUnitPromoteDimension,
                                    ExecuteUnaryNegate};

        const OpTraits kOpNot = {1,
                                 "not",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeAlwaysInt,
                                 DeriveUnitDimless,
                                 ExecuteUnaryNot};

        const OpTraits kOpBitNot = {1,
                                 "bitnot",
                                 DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypeRequireInt,
                                    DeriveUnitPromoteDimension,
                                    ExecuteUnaryBitNot};

        // ---- ternary ---------------------------------------------------------------

        const OpTraits kOpConditional = {3,
                                         "conditional",
                                         DeriveShapeBroadcast,
                                         DeriveRowsBroadcast,
                                         DeriveDtypeConditional,
                                         DeriveUnitConditional,
                                         ExecuteConditional};

        const OpTraits kOpIf = {-1,
                                "if",
                                DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypeIf,
                                DeriveUnitIf,
                                ExecuteIf};

        // ---- variadic --------------------------------------------------------------

        const OpTraits kOpSweep = {-1,
                                 "sweep",
                                 DeriveShapeBroadcast,
                                   DeriveRowsSum,
                                   DeriveDtypePromoteWithString,
                                   DeriveUnitPromoteDimension,
                                   ExecuteSweep};

        const OpTraits kOpMatrix = {-1,
                                 "matrix",
                                 DeriveShapeMatrix,
                                    DeriveRowsBroadcast,
                                    DeriveDtypePromoteWithString,
                                    DeriveUnitPromoteDimension,
                                    ExecuteMatrix};

        // =========================================================================
        //  Public API wrappers
        // =========================================================================

        Value OperationAdd(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpAdd);
        }
        Value OperationSub(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpSub);
        }
        Value OperationMul(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpMul);
        }
        Value OperationDiv(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpDiv);
        }
        Value OperationTimes(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpTimes);
        }
        Value OperationRdivide(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpRdivide);
        }
        Value OperationMod(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpMod);
        }
        Value OperationPow(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpPow);
        }

        Value OperationEq(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpEq);
        }
        Value OperationNeq(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpNeq);
        }
        Value OperationLt(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpLt);
        }
        Value OperationGt(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpGt);
        }
        Value OperationLe(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpLe);
        }
        Value OperationGe(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpGe);
        }

        Value OperationAnd(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpAnd);
        }
        Value OperationOr(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpOr);
        }

        Value OperationBitAnd(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpBitAnd);
        }
        Value OperationBitOr(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpBitOr);
        }
        Value OperationBitXor(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpBitXor);
        }

        Value OperationShl(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpShl);
        }
        Value OperationShr(const Value& lhs, const Value& rhs)
        {
            return Operate({lhs, rhs}, kOpShr);
        }

        Value OperationNegate(const Value& v)
        {
            return Operate({v}, kOpNegate);
        }
        Value OperationNot(const Value& v)
        {
            return Operate({v}, kOpNot);
        }
        Value OperationBitNot(const Value& v)
        {
            return Operate({v}, kOpBitNot);
        }

        Value OperationConditional(const Value& condition,
                                   const Value& true_value,
                                   const Value& false_value)
        {
            return Operate({condition, true_value, false_value}, kOpConditional);
        }

        Value OperationIf(const std::vector<Value>& ops)
        {
            return Operate(ops, kOpIf);
        }

        Value OperationMatrix(const std::vector<Value>& ops)
        {
            return Operate(ops, kOpMatrix);
        }
        Value OperationSweep(const std::vector<Value>& ops)
        {
            return Operate(ops, kOpSweep);
        }

    } // namespace operation
} // namespace rel
