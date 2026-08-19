// =============================================================================
//  REL -- Math functions implemented via the Operate pipeline
// =============================================================================

#include "operation/math_operation.h"
#include "data_array.h"
#include "data_series.h"
#include "operation/operation_helpers.h"
#include "operation/pipeline.h"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rel
{
    namespace operation
    {

        using namespace xdataset;

        // =========================================================================
        //  Element-wise math ops
        // =========================================================================

        using C = std::complex<double>;

        namespace
        {

            template <typename T>
            static inline T op_sin(T x)
            {
                return std::sin(x);
            }
            template <typename T>
            static inline T op_cos(T x)
            {
                return std::cos(x);
            }
            template <typename T>
            static inline T op_tan(T x)
            {
                return std::tan(x);
            }
            template <typename T>
            static inline T op_cot(T x)
            {
                return T(1.0) / std::tan(x);
            }
            template <typename T>
            static inline T op_asin(T x)
            {
                return std::asin(x);
            }
            template <typename T>
            static inline T op_acos(T x)
            {
                return std::acos(x);
            }
            template <typename T>
            static inline T op_atan(T x)
            {
                return std::atan(x);
            }
            template <typename T>
            static inline T op_acot(T x)
            {
                return std::atan(T(1.0) / x);
            }
            template <typename T>
            static inline T op_sinh(T x)
            {
                return std::sinh(x);
            }
            template <typename T>
            static inline T op_cosh(T x)
            {
                return std::cosh(x);
            }
            template <typename T>
            static inline T op_tanh(T x)
            {
                return std::tanh(x);
            }
            template <typename T>
            static inline T op_coth(T x)
            {
                return T(1.0) / std::tanh(x);
            }
            template <typename T>
            static inline T op_asinh(T x)
            {
                return std::asinh(x);
            }
            template <typename T>
            static inline T op_acosh(T x)
            {
                return std::acosh(x);
            }
            template <typename T>
            static inline T op_atanh(T x)
            {
                return std::atanh(x);
            }
            template <typename T>
            static inline T op_acoth(T x)
            {
                return std::atanh(T(1.0) / x);
            }
            template <typename T>
            static inline T op_log(T x)
            {
                return std::log(x);
            }
            template <typename T>
            static inline T op_log10(T x)
            {
                return std::log10(x);
            }
            template <typename T>
            static inline T op_exp(T x)
            {
                return std::exp(x);
            }
            template <typename T>
            static inline T op_sqrt(T x)
            {
                return std::sqrt(x);
            }
            template <typename T>
            static inline T op_sqr(T x)
            {
                return x * x;
            }

            // --- T -> double ------------------------------------------------------
            template <typename T>
            static inline double op_abs(T x)
            {
                return std::abs(x);
            }

            // --- sinc / step ------------------------------------------------------
            template <typename T>
            static inline T op_sinc(T x)
            {
                const T px = T(M_PI) * x;
                return (std::abs(x) < T(1e-15)) ? T(1.0) : std::sin(px) / px;
            }
            template <>
            inline C op_sinc<C>(C x)
            {
                double a = std::abs(x);
                if (a < 1e-15)
                    return C(1.0, 0.0);
                const C px = M_PI * x;
                return std::sin(px) / px;
            }

            // --- db / dbm ---------------------------------------------------------
            template <typename T>
            static inline double op_db(double r_mag, T z1, T z2)
            {
                double z_out = (std::abs(z2) * std::abs(z2)) / std::real(z2);
                double z_in = (std::abs(z1) * std::abs(z1)) / std::real(z1);
                return 20.0 * std::log10(r_mag) - 10.0 * std::log10(z_out / z_in);
            }
            static inline double op_dbm(double v_mag, C z)
            {
                return 20.0 * std::log10(v_mag) - 10.0 * std::log10(std::abs(z) / 50.0) + 10.0;
            }

            template <typename T>
            static inline T op_dbmtow(T x)
            {
                return std::pow(T(10.0), x / T(10.0)) * T(0.001);
            }

            template <typename T>
            static inline T op_wtodbm(T x)
            {
                return T(10.0) * std::log10(x * T(1000.0));
            }

            // --- dbm / W unit derives ---------------------------------------------
            static inline Unit DeriveUnitDbmtow(const std::vector<Unit>& units)
            {
                if (units[0].has_dimension())
                    throw std::invalid_argument("input must be dimensionless");
                return Unit::parse("W");
            }
            static inline Unit DeriveUnitWtodbm(const std::vector<Unit>& units)
            {
                if (!units[0].has_dimension() || !units[0].same_dimension(Unit::parse("W")))
                    throw std::invalid_argument("input unit must be W");
                return Unit();
            }
            static inline Unit DeriveUnitDb(const std::vector<Unit>& units)
            {
                if (units[0].has_dimension())
                    throw std::invalid_argument("r must be dimensionless");
                Unit ohm = Unit::parse("Ohm");
                for (size_t i = 1; i < units.size(); ++i)
                {
                    if (units[i].has_dimension() && !units[i].same_dimension(ohm))
                        throw std::invalid_argument(
                            "z1 and z2 must be dimensionless or in Ohm");
                }
                if (!units[1].same_dimension(units[2]))
                    throw std::invalid_argument("z1 and z2 must have the same unit");
                return Unit();
            }
            static inline Unit DeriveUnitDbm(const std::vector<Unit>& units)
            {
                Unit volt = Unit::parse("V");
                Unit ohm = Unit::parse("Ohm");
                if (units[0].has_dimension() && !units[0].same_dimension(volt))
                    throw std::invalid_argument("v must be dimensionless or in V");
                if (units[1].has_dimension() && !units[1].same_dimension(ohm))
                    throw std::invalid_argument("z must be dimensionless or in Ohm");
                return Unit();
            }

            // --- binary helpers ---------------------------------------------------
            template <typename T>
            static inline T op_root(T x, T n)
            {
                return std::pow(x, T(1.0) / n);
            }
            template <typename T>
            static inline T op_max2(T a, T b)
            {
                return a > b ? a : b;
            }
            template <typename T>
            static inline T op_min2(T a, T b)
            {
                return a < b ? a : b;
            }
            template <>
            inline C op_max2<C>(C a, C b)
            {
                return std::abs(a) > std::abs(b) ? a : b;
            }
            template <>
            inline C op_min2<C>(C a, C b)
            {
                return std::abs(a) < std::abs(b) ? a : b;
            }

        } // anonymous namespace

        // =========================================================================
        //  Execute callbacks
        // =========================================================================

        Value ExecuteSin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sin<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        Value ExecuteCos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cos<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteTan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tan<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteCot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cot<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cot<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAsin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asin<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAcos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acos<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAtan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atan<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAcot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acot<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acot<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteSinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sinh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteCosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cosh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteTanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tanh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteCoth(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_coth<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_coth<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAsinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asinh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAcosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acosh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAtanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atanh<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteAcoth(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acoth<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acoth<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteLog(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteLog10(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log10<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log10<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteExp(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_exp<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_exp<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteSqrt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqrt<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqrt<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteSqr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, op_sqr<int>);
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqr<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqr<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteSgn(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, int>(
                info, ops, [](double x) { return (x > 0.0)   ? 1
                                                 : (x < 0.0) ? -1
                                                             : 0; });
        }
        Value ExecuteCeil(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, int>(
                info, ops, [](double x) { return static_cast<int>(std::ceil(x)); });
        }
        Value ExecuteFloor(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, int>(
                info, ops, [](double x) { return static_cast<int>(std::floor(x)); });
        }
        Value ExecuteRound(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, int>(
                info, ops, [](double x) { return static_cast<int>(std::round(x)); });
        }
        Value ExecuteDeg(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryT<double>(info, ops, [](double x) { return x * (180.0 / M_PI); });
        }
        Value ExecuteRad(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryT<double>(info, ops, [](double x) { return x * (M_PI / 180.0); });
        }

        Value ExecuteStep(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryT<double>(info, ops,
                [](double x) { return (x > 0.0) ? 1.0 : (x < 0.0) ? 0.0 : 0.5; });
        }

        Value ExecuteAbs(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger:
                    return ExecUnaryT<int>(info, ops, [](int x) { return std::abs(x); });
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_abs<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_abs<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteReal(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, [](int x) { return x; });
                case DataType::kReal:
                    return ExecUnaryT<double>(info, ops, [](double x) { return x; });
                case DataType::kComplex:
                    return ExecUnaryCT<C, double>(info, ops, [](C x) { return std::real(x); });
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteImag(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, [](int) { return 0; });
                case DataType::kReal:
                    return ExecUnaryT<double>(info, ops, [](double) { return 0.0; });
                case DataType::kComplex:
                    return ExecUnaryCT<C, double>(info, ops, [](C x) { return std::imag(x); });
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecutePhase(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<C, double>(
                info, ops, [](C x) { return std::arg(x) * (180.0 / M_PI); });
        }
        Value ExecuteConj(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, [](int x) { return x; });
                case DataType::kReal:
                    return ExecUnaryT<double>(info, ops, [](double x) { return x; });
                case DataType::kComplex:
                    return ExecUnaryT<C>(info, ops, [](C x) { return std::conj(x); });
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteInt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, int>(
                info, ops, [](double x) { return static_cast<int>(x); });
        }
        Value ExecuteFloat(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryT<double>(info, ops, [](double x) { return x; });
        }
        Value ExecuteSinc(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sinc<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sinc<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteDbmtow(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_dbmtow<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_dbmtow<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteWtodbm(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_wtodbm<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_wtodbm<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // multi-arg callbacks
        Value ExecuteDb(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            auto v_flat = ops[0].flat_data<C>();
            auto z1_flat = ops[1].flat_data<C>();
            auto z2_flat = ops[2].flat_data<C>();
            const C* v_ptr = v_flat.ptr;
            const C* z1_ptr = z1_flat.ptr;
            const C* z2_ptr = z2_flat.ptr;
            Index v_stride = v_flat.stride;
            Index z1_stride = z1_flat.stride;
            Index z2_stride = z2_flat.stride;

            ShapeBroadcastPlan sp = ShapeBroadcastPlan::Make(
                {ops[0].data_shape(), ops[1].data_shape(), ops[2].data_shape()}, info.shape);
            RowBroadcastPlan rp =
                RowBroadcastPlan::Compute({ops[0].rows(), ops[1].rows(), ops[2].rows()});

            auto out_ds = std::unique_ptr<DataSeries>(new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index os = sp.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index v_off = (rp.broadcast[0] ? 0 : i) * v_stride;
                Index z1_off = (rp.broadcast[1] ? 0 : i) * z1_stride;
                Index z2_off = (rp.broadcast[2] ? 0 : i) * z2_stride;
                Index o_off = i * os;
                for (Index j = 0; j < os; ++j)
                {
                    out[o_off + j] = op_db(std::abs(v_ptr[v_off + sp.MapFlatIndex(j, 0)]),
                                           z1_ptr[z1_off + sp.MapFlatIndex(j, 1)],
                                           z2_ptr[z2_off + sp.MapFlatIndex(j, 2)]);
                }
            }
            if (ops[0].is_measurement() && ops[1].is_measurement() && ops[2].is_measurement())
                return Value(out_ds->measurement_at(0));
            const DataArray* src = &ops[0].as_data_array();
            auto da = std::make_shared<DataArray>(src->clone());
            da->set_data(std::move(*out_ds));
            return Value(da);
        }
        Value ExecuteDbm(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            auto l_flat = ops[0].flat_data<C>();
            auto r_flat = ops[1].flat_data<C>();
            const C* l_ptr = l_flat.ptr;
            const C* r_ptr = r_flat.ptr;
            Index ls = l_flat.stride;
            Index rs = r_flat.stride;

            ShapeBroadcastPlan sp =
                ShapeBroadcastPlan::Make({ops[0].data_shape(), ops[1].data_shape()}, info.shape);
            RowBroadcastPlan rp = RowBroadcastPlan::Compute({ops[0].rows(), ops[1].rows()});

            auto out_ds = std::unique_ptr<DataSeries>(new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(Unit());
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index os = sp.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index lo = (rp.broadcast[0] ? 0 : i) * ls;
                Index ro = (rp.broadcast[1] ? 0 : i) * rs;
                Index oo = i * os;
                for (Index j = 0; j < os; ++j)
                {
                    out[oo + j] = op_dbm(std::abs(l_ptr[lo + sp.MapFlatIndex(j, 0)]),
                                         r_ptr[ro + sp.MapFlatIndex(j, 1)]);
                }
            }
            if (ops[0].is_measurement() && ops[1].is_measurement())
                return Value(out_ds->measurement_at(0));
            const DataArray* src = &ops[0].as_data_array();
            auto da = std::make_shared<DataArray>(src->clone());
            da->set_data(std::move(*out_ds));
            return Value(da);
        }

        // binary
        Value ExecuteAtan2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<double>(
                info, ops, [](double y, double x) { return std::atan2(y, x); });
        }
        Value ExecuteRoot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_root<double>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_root<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteMax2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_max2<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_max2<int>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_max2<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }
        Value ExecuteMin2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_min2<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_min2<int>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_min2<C>);
                default: throw std::invalid_argument("unsupported dtype");
            }
        }

        // =========================================================================
        //  OpTraits definitions
        // =========================================================================

        const OpTraits kOpSin = {1,
                                 "sin",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteSin};
        const OpTraits kOpCos = {1,
                                 "cos",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteCos};
        const OpTraits kOpTan = {1,
                                 "tan",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteTan};
        const OpTraits kOpCot = {1,
                                 "cot",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteCot};
        const OpTraits kOpAsin = {1,
                                  "asin",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteAsin};
        const OpTraits kOpAcos = {1,
                                  "acos",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteAcos};
        const OpTraits kOpAtan = {1,
                                  "atan",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteAtan};
        const OpTraits kOpAcot = {1,
                                  "acot",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteAcot};
        const OpTraits kOpSinh = {1,
                                  "sinh",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteSinh};
        const OpTraits kOpCosh = {1,
                                  "cosh",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteCosh};
        const OpTraits kOpTanh = {1,
                                  "tanh",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteTanh};
        const OpTraits kOpCoth = {1,
                                  "coth",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteCoth};
        const OpTraits kOpAsinh = {1,
                                   "asinh",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromoteReal,
                                   DeriveUnitPromoteDimension,
                                   ExecuteAsinh};
        const OpTraits kOpAcosh = {1,
                                   "acosh",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromoteReal,
                                   DeriveUnitPromoteDimension,
                                   ExecuteAcosh};
        const OpTraits kOpAtanh = {1,
                                   "atanh",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromoteReal,
                                   DeriveUnitPromoteDimension,
                                   ExecuteAtanh};
        const OpTraits kOpAcoth = {1,
                                   "acoth",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromoteReal,
                                   DeriveUnitPromoteDimension,
                                   ExecuteAcoth};
        const OpTraits kOpLog = {1,
                                 "log",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteLog};
        const OpTraits kOpLog10 = {1,
                                   "log10",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypePromoteReal,
                                   DeriveUnitPromoteDimension,
                                   ExecuteLog10};
        const OpTraits kOpExp = {1,
                                 "exp",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromoteReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteExp};
        const OpTraits kOpSqrt = {1,
                                  "sqrt",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteSqrt};
        const OpTraits kOpSqr = {1,
                                 "sqr",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypePromote,
                                 DeriveUnitSquare,
                                 ExecuteSqr};
        const OpTraits kOpSgn = {1,
                                 "sgn",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceIntNoComplex,
                                 DeriveUnitDimless,
                                 ExecuteSgn};
        const OpTraits kOpCeil = {1,
                                  "ceil",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeForceIntNoComplex,
                                  DeriveUnitPromoteDimension,
                                  ExecuteCeil};
        const OpTraits kOpFloor = {1,
                                   "floor",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceIntNoComplex,
                                   DeriveUnitPromoteDimension,
                                   ExecuteFloor};
        const OpTraits kOpRound = {1,
                                   "round",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceIntNoComplex,
                                   DeriveUnitPromoteDimension,
                                   ExecuteRound};
        const OpTraits kOpDeg = {1,
                                 "deg",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceRealNoComplex,
                                 DeriveUnitPromoteDimension,
                                 ExecuteDeg};
        const OpTraits kOpRad = {1,
                                 "rad",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceRealNoComplex,
                                 DeriveUnitPromoteDimension,
                                 ExecuteRad};

        const OpTraits kOpAbs = {1,
                                 "abs",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeComplexToReal,
                                 DeriveUnitPromoteDimension,
                                 ExecuteAbs};
        const OpTraits kOpReal = {1,
                                  "real",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeComplexToReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteReal};
        const OpTraits kOpImag = {1,
                                  "imag",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeComplexToReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteImag};
        const OpTraits kOpPhase = {1,
                                   "phase",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceReal,
                                   DeriveUnitDimless,
                                   ExecutePhase};
        const OpTraits kOpConj = {1,
                                  "conj",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromote,
                                  DeriveUnitPromoteDimension,
                                  ExecuteConj};

        const OpTraits kOpInt = {1,
                                 "int",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceIntNoComplex,
                                 DeriveUnitPromoteDimension,
                                 ExecuteInt};
        const OpTraits kOpFloat = {1,
                                   "float",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceRealNoComplex,
                                   DeriveUnitPromoteDimension,
                                   ExecuteFloat};
        const OpTraits kOpSinc = {1,
                                  "sinc",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteSinc};
        const OpTraits kOpStep = {1,
                                  "step",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeForceRealNoComplex,
                                  DeriveUnitDimless,
                                  ExecuteStep};

        const OpTraits kOpDbmtow = {1,
                                    "dbmtow",
                                    DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypePromoteReal,
                                    DeriveUnitDbmtow,
                                    ExecuteDbmtow};
        const OpTraits kOpWtodbm = {1,
                                    "wtodbm",
                                    DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypePromoteReal,
                                    DeriveUnitWtodbm,
                                    ExecuteWtodbm};
        const OpTraits kOpDb = {3,
                                "db",
                                DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypeForceReal,
                                DeriveUnitDb,
                                ExecuteDb};
        const OpTraits kOpDbm = {2,
                                 "dbm",
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceReal,
                                 DeriveUnitDbm,
                                 ExecuteDbm};

        const OpTraits kOpAtan2 = {2,
                                   "atan2",
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceRealNoComplex,
                                   DeriveUnitPromoteDimension,
                                   ExecuteAtan2};
        const OpTraits kOpRoot = {2,
                                  "root",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromoteReal,
                                  DeriveUnitPromoteDimension,
                                  ExecuteRoot};
        const OpTraits kOpMax2 = {2,
                                  "max2",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromote,
                                  DeriveUnitPromoteDimension,
                                  ExecuteMax2};
        const OpTraits kOpMin2 = {2,
                                  "min2",
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromote,
                                  DeriveUnitPromoteDimension,
                                  ExecuteMin2};

        // =========================================================================
        //  Public API wrappers
        // =========================================================================

        Value OperationSin(const Value& v)
        {
            return Operate({v}, kOpSin);
        }
        Value OperationCos(const Value& v)
        {
            return Operate({v}, kOpCos);
        }
        Value OperationTan(const Value& v)
        {
            return Operate({v}, kOpTan);
        }
        Value OperationCot(const Value& v)
        {
            return Operate({v}, kOpCot);
        }
        Value OperationAsin(const Value& v)
        {
            return Operate({v}, kOpAsin);
        }
        Value OperationAcos(const Value& v)
        {
            return Operate({v}, kOpAcos);
        }
        Value OperationAtan(const Value& v)
        {
            return Operate({v}, kOpAtan);
        }
        Value OperationAcot(const Value& v)
        {
            return Operate({v}, kOpAcot);
        }
        Value OperationSinh(const Value& v)
        {
            return Operate({v}, kOpSinh);
        }
        Value OperationCosh(const Value& v)
        {
            return Operate({v}, kOpCosh);
        }
        Value OperationTanh(const Value& v)
        {
            return Operate({v}, kOpTanh);
        }
        Value OperationCoth(const Value& v)
        {
            return Operate({v}, kOpCoth);
        }
        Value OperationAsinh(const Value& v)
        {
            return Operate({v}, kOpAsinh);
        }
        Value OperationAcosh(const Value& v)
        {
            return Operate({v}, kOpAcosh);
        }
        Value OperationAtanh(const Value& v)
        {
            return Operate({v}, kOpAtanh);
        }
        Value OperationAcoth(const Value& v)
        {
            return Operate({v}, kOpAcoth);
        }
        Value OperationLog(const Value& v)
        {
            return Operate({v}, kOpLog);
        }
        Value OperationLog10(const Value& v)
        {
            return Operate({v}, kOpLog10);
        }
        Value OperationExp(const Value& v)
        {
            return Operate({v}, kOpExp);
        }
        Value OperationSqrt(const Value& v)
        {
            return Operate({v}, kOpSqrt);
        }
        Value OperationSqr(const Value& v)
        {
            return Operate({v}, kOpSqr);
        }
        Value OperationCeil(const Value& v)
        {
            return Operate({v}, kOpCeil);
        }
        Value OperationFloor(const Value& v)
        {
            return Operate({v}, kOpFloor);
        }
        Value OperationRound(const Value& v)
        {
            return Operate({v}, kOpRound);
        }
        Value OperationDeg(const Value& v)
        {
            return Operate({v}, kOpDeg);
        }
        Value OperationRad(const Value& v)
        {
            return Operate({v}, kOpRad);
        }
        Value OperationAbs(const Value& v)
        {
            return Operate({v}, kOpAbs);
        }
        Value OperationSgn(const Value& v)
        {
            return Operate({v}, kOpSgn);
        }
        Value OperationReal(const Value& v)
        {
            return Operate({v}, kOpReal);
        }
        Value OperationImag(const Value& v)
        {
            return Operate({v}, kOpImag);
        }
        Value OperationConj(const Value& v)
        {
            return Operate({v}, kOpConj);
        }
        Value OperationPhase(const Value& v)
        {
            return Operate({v}, kOpPhase);
        }
        Value OperationInt(const Value& v)
        {
            return Operate({v}, kOpInt);
        }
        Value OperationFloat(const Value& v)
        {
            return Operate({v}, kOpFloat);
        }
        Value OperationSinc(const Value& v)
        {
            return Operate({v}, kOpSinc);
        }
        Value OperationStep(const Value& v)
        {
            return Operate({v}, kOpStep);
        }
        Value OperationDbmtow(const Value& v)
        {
            return Operate({v}, kOpDbmtow);
        }
        Value OperationWtodbm(const Value& v)
        {
            return Operate({v}, kOpWtodbm);
        }

        Value OperationAtan2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpAtan2);
        }
        Value OperationRoot(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpRoot);
        }
        Value OperationMax2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpMax2);
        }
        Value OperationMin2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpMin2);
        }
        Value OperationDb(const Value& a, const Value& b, const Value& c)
        {
            return Operate({a, b, c}, kOpDb);
        }
        Value OperationDbm(const Value& v, const Value& z)
        {
            return Operate({v, z}, kOpDbm);
        }

    } // namespace operation
} // namespace rel
