// Math library: element-wise unary math functions.
//
// Implemented via the Operate pipeline (operation/pipeline.h /
// operation/math_operation.h).  Each function delegates to
// rel::operation::OperationXxx, which runs through the full
// derive + broadcast + execute flow.
//
// Type promotion: Boolean -> Integer (0/1), Integer -> Real promotion
// is handled by DeriveDtypeMath in the pipeline.

#include "math_library.h"
#include "operation/math_operation.h"
#include "operation/operator.h"
#include "value.h"

#include <cmath>
#include <complex>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace xdataset;
using namespace rel::operation;

namespace rel
{
namespace math
{

    // ---- Function factory helpers ----------------------------------------------------

    Function make_unary_fn(const char* name, Value (*fn)(const Value&))
    {
        return Function(name,
                        std::vector<FunctionParam>{Param("x")},
                        [name, fn](const Function::ArgMap& args) -> Value
                        { return fn(args.at("x")); });
    }

    Function make_binary_fn(const char* name, Value (*fn)(const Value&, const Value&))
    {
        return Function(name,
                        std::vector<FunctionParam>{Param("x"), Param("y")},
                        [name, fn](const Function::ArgMap& args) -> Value
                        { return fn(args.at("x"), args.at("y")); });
    }

    // ---- Reduce helpers --------------------------------------------------------------

    /// Three-way comparison for scalar values via DataSeries::ConstRowView (zero-allocation).
    /// Numeric: value compare. Complex: abs compare. Boolean: false<true.
    /// String: lexicographic.
    int CmpView(const DataSeries::ConstRowView& a, const DataSeries::ConstRowView& b)
    {
        switch (a.data_type())
        {
            case DataType::kReal:
            {
                double x = a.scalar<double>();
                double y = b.scalar<double>();
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            case DataType::kInteger:
            {
                int x = a.scalar<int>();
                int y = b.scalar<int>();
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            case DataType::kComplex:
            {
                double x = std::abs(a.scalar<std::complex<double>>());
                double y = std::abs(b.scalar<std::complex<double>>());
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            case DataType::kString:
            {
                const std::string& x = a.scalar<std::string>();
                const std::string& y = b.scalar<std::string>();
                return x < y ? -1 : (x > y ? 1 : 0);
            }
            default:
                return 0;
        }
    }

    /// Callback: reduce one group of leaf rows [start, end) into a single Measurement.
    using ReduceGroupFn = std::function<Measurement(Index, Index, const DataSeries&)>;

    /// Reduce along the innermost dimension of a Value.
    ///
    /// @param v            source Value (Measurement or DataArray, must have scalar data, rank >= 1)
    /// @param src          pre-extracted self DataSeries via v.data()
    /// @param out_dtype    dtype of the output DataSeries
    /// @param out_unit     unit of the output DataSeries
    /// @param reduce_group callback: Measurement(Index flat_start, Index flat_end,
    ///                     const DataSeries& src)
    Value ReduceInnermost(const Value& v,
                           const DataSeries& src,
                           DataType out_dtype,
                           const Unit& out_unit,
                           ReduceGroupFn reduce_group)
    {
        const MultiDimensionSpec& spec = v.dimension_spec();
        const std::size_t rank = spec.rank();

        if (rank == 0)
            throw std::logic_error("reduce requires at least one dimension");
        if (src.data_kind() != DataKind::kScalar)
            throw std::logic_error("reduce is only supported for scalar data");

        DataSeries out(out_dtype, DataShape::Scalar());
        out.set_unit(out_unit);

        if (rank >= 2)
        {
            v.for_each_indep_group(1,
                [&](const MultiDimensionSpec::DimGroup& g)
                { out.append(reduce_group(g.flat_start, g.flat_end, src)); });
        }
        else
        {
            Index total = v.rows();
            out.append(reduce_group(0, total, src));
        }

        DataArrayCreateInfo info;
        if (rank >= 2)
        {
            info.kind = DataArrayKind::kDependent;
            const auto& names = v.indep_names();
            for (std::size_t i = 0; i < rank - 1; ++i)
                info.datas[names[i]] = v.indep_data(static_cast<Index>(rank - i));
            info.datas[DataArray::kSelf] = std::move(out);

            MultiDimensionSpec result_spec;
            for (std::size_t i = 0; i < rank - 1; ++i)
                result_spec.add_dimension(spec.dim(static_cast<Index>(i)));
            info.multi_dimension_spec = std::move(result_spec);
        }
        else
        {
            info.kind = DataArrayKind::kIndependent;
            info.datas[DataArray::kSelf] = std::move(out);
            info.multi_dimension_spec = MultiDimensionSpec().add_regular(1);
        }

        return Value(std::make_shared<DataArray>(std::move(info)));
    }

    // ---- Reduce operations ------------------------------------------------------------

    /// Innermost-dimension minimum.
    Value Min(const Value& v)
    {
        DataSeries src = v.data();

        auto result = ReduceInnermost(v, src, src.data_type(), src.unit(),
            [&](Index start, Index end, const DataSeries& src_data) -> Measurement
            {
                DataSeries::ConstRowView best_view(nullptr, 0);
                bool first = true;
                v.for_each_leaf_row(
                    [&](const MultiDimensionSpec::LeafRow& leaf)
                    {
                        DataSeries::ConstRowView rv = src_data.row(leaf.row_flat);
                        if (first) { best_view = rv; first = false; }
                        else if (CmpView(rv, best_view) < 0) best_view = rv;
                    },
                    start, end);
                return best_view.to_measurement();
            });

        return result;
    }

    /// Innermost-dimension maximum.
    Value Max(const Value& v)
    {
        DataSeries src = v.data();

        auto result = ReduceInnermost(v, src, src.data_type(), src.unit(),
            [&](Index start, Index end, const DataSeries& src_data) -> Measurement
            {
                DataSeries::ConstRowView best_view(nullptr, 0);
                bool first = true;
                v.for_each_leaf_row(
                    [&](const MultiDimensionSpec::LeafRow& leaf)
                    {
                        DataSeries::ConstRowView rv = src_data.row(leaf.row_flat);
                        if (first) { best_view = rv; first = false; }
                        else if (CmpView(rv, best_view) > 0) best_view = rv;
                    },
                    start, end);
                return best_view.to_measurement();
            });

        return result;
    }

    /// Innermost-dimension sum.
    Value Sum(const Value& v)
    {
        DataSeries src = v.data();
        DataType dt = src.data_type();

        switch (dt)
        {
            case DataType::kReal:
                return ReduceInnermost(v, src, DataType::kReal, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        double acc = 0.0;
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += src_data.row(leaf.row_flat).scalar<double>();
                            },
                            start, end);
                        return Measurement::Real(acc, src.unit());
                    });
            case DataType::kInteger:
                return ReduceInnermost(v, src, DataType::kInteger, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        int acc = 0;
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += src_data.row(leaf.row_flat).scalar<int>();
                            },
                            start, end);
                        return Measurement::Integer(acc, src.unit());
                    });
            case DataType::kComplex:
            {
                using C = std::complex<double>;
                return ReduceInnermost(v, src, DataType::kComplex, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        C acc(0.0, 0.0);
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += src_data.row(leaf.row_flat).scalar<C>();
                            },
                            start, end);
                        return Measurement::Complex(acc, src.unit());
                    });
            }
            default:
                throw std::runtime_error("sum: unsupported data type");
        }
    }

    /// Innermost-dimension mean.
    Value Mean(const Value& v)
    {
        DataSeries src = v.data();
        DataType dt = src.data_type();

        switch (dt)
        {
            case DataType::kReal:
                return ReduceInnermost(v, src, DataType::kReal, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        double acc = 0.0;
                        Index cnt = 0;
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += src_data.row(leaf.row_flat).scalar<double>();
                                ++cnt;
                            },
                            start, end);
                        return Measurement::Real(
                            cnt > 0 ? acc / static_cast<double>(cnt) : 0.0, src.unit());
                    });
            case DataType::kInteger:
                return ReduceInnermost(v, src, DataType::kReal, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        double acc = 0.0;
                        Index cnt = 0;
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += static_cast<double>(
                                    src_data.row(leaf.row_flat).scalar<int>());
                                ++cnt;
                            },
                            start, end);
                        return Measurement::Real(
                            cnt > 0 ? acc / static_cast<double>(cnt) : 0.0, src.unit());
                    });
            case DataType::kComplex:
            {
                using C = std::complex<double>;
                return ReduceInnermost(v, src, DataType::kComplex, src.unit(),
                    [&](Index start, Index end, const DataSeries& src_data) -> Measurement {
                        C acc(0.0, 0.0);
                        Index cnt = 0;
                        v.for_each_leaf_row(
                            [&](const MultiDimensionSpec::LeafRow& leaf) {
                                acc += src_data.row(leaf.row_flat).scalar<C>();
                                ++cnt;
                            },
                            start, end);
                        double n = static_cast<double>(cnt);
                        return Measurement::Complex(
                            cnt > 0 ? acc / n : C(0.0, 0.0), src.unit());
                    });
            }
            default:
                throw std::runtime_error("mean: unsupported data type");
        }
    }

    // ---- Library definition ----------------------------------------------------------

    FunctionLibrary MakeLibrary()
    {
        FunctionLibrary lib("math");

        // Trigonometric
        lib.Add(make_unary_fn("sin",  OperationSin));
        lib.Add(make_unary_fn("cos",  OperationCos));
        lib.Add(make_unary_fn("tan",  OperationTan));
        lib.Add(make_unary_fn("cot",  OperationCot));

        // Inverse trigonometric
        lib.Add(make_unary_fn("asin",  OperationAsin));
        lib.Add(make_unary_fn("acos",  OperationAcos));
        lib.Add(make_unary_fn("atan",  OperationAtan));
        lib.Add(make_unary_fn("acot",  OperationAcot));

        // Hyperbolic
        lib.Add(make_unary_fn("sinh",  OperationSinh));
        lib.Add(make_unary_fn("cosh",  OperationCosh));
        lib.Add(make_unary_fn("tanh",  OperationTanh));
        lib.Add(make_unary_fn("coth",  OperationCoth));

        // Inverse hyperbolic
        lib.Add(make_unary_fn("asinh", OperationAsinh));
        lib.Add(make_unary_fn("acosh", OperationAcosh));
        lib.Add(make_unary_fn("atanh", OperationAtanh));
        lib.Add(make_unary_fn("acoth", OperationAcoth));

        // Logarithms & exponential
        lib.Add(make_unary_fn("log", OperationLog));
        lib.Add(make_unary_fn("ln", OperationLog));
        lib.Add(make_unary_fn("log10", OperationLog10));
        lib.Add(make_unary_fn("exp", OperationExp));

        // Power
        lib.Add(make_unary_fn("sqrt", OperationSqrt));
        lib.Add(make_unary_fn("sqr", OperationSqr));
        // Rounding & casting
        lib.Add(make_unary_fn("ceil",  OperationCeil));
        lib.Add(make_unary_fn("floor", OperationFloor));
        lib.Add(make_unary_fn("round", OperationRound));
        lib.Add(make_unary_fn("cint", OperationRound));
        lib.Add(make_unary_fn("fix",   OperationInt));
        lib.Add(make_unary_fn("int",   OperationInt));
        lib.Add(make_unary_fn("float", OperationFloat));

        // Angle conversion
        lib.Add(make_unary_fn("deg", OperationDeg));
        lib.Add(make_unary_fn("rad", OperationRad));

        // Misc
        lib.Add(make_unary_fn("sinc", OperationSinc));
        lib.Add(make_unary_fn("step", OperationStep));
        // Absolute & sign
        lib.Add(make_unary_fn("abs", OperationAbs));
        lib.Add(make_unary_fn("sgn", OperationSgn));

        // Complex number operations
        lib.Add(make_unary_fn("real", OperationReal));
        lib.Add(make_unary_fn("re", OperationReal));
        lib.Add(make_unary_fn("imag", OperationImag));
        lib.Add(make_unary_fn("im", OperationImag));
        lib.Add(make_unary_fn("conj", OperationConj));
        lib.Add(make_unary_fn("conjg", OperationConj));
        lib.Add(make_unary_fn("mag", OperationAbs));
        lib.Add(make_unary_fn("phase", OperationPhase));

        // dB conversions
        lib.Add(Function("db",
            std::vector<FunctionParam>{
                Param("r"),
                Param("z1", Value::Real(50, Unit::parse("Ohm"))),
                Param("z2", Value::Real(50, Unit::parse("Ohm"))),
            },
            [](const Function::ArgMap& args) {
                return OperationDb(args.at("r"), args.at("z1"), args.at("z2"));
            }));
        lib.Add(Function("dbm",
            std::vector<FunctionParam>{Param("v"), Param("z", Value::Real(50))},
            [](const Function::ArgMap& args) {
                return OperationDbm(args.at("v"), args.at("z"));
            }));
        lib.Add(make_unary_fn("dbmtow",  OperationDbmtow));
        lib.Add(make_unary_fn("wtodbm",  OperationWtodbm));

        // Operation kernels (Value operators registered as callable functions)
        // -- binary arithmetic
        lib.Add(make_binary_fn("add",       OperationAdd));
        lib.Add(make_binary_fn("subtract",  OperationSub));
        lib.Add(make_binary_fn("multiply",  OperationMul));
        lib.Add(make_binary_fn("divide",    OperationDiv));
        lib.Add(make_binary_fn("times",     OperationTimes));
        lib.Add(make_binary_fn("rdivide",   OperationRdivide));
        lib.Add(make_binary_fn("mod",       OperationMod));

        // -- comparison
        lib.Add(make_binary_fn("equal",        OperationEq));
        lib.Add(make_binary_fn("notequal",     OperationNeq));
        lib.Add(make_binary_fn("lessthan",     OperationLt));
        lib.Add(make_binary_fn("greaterthan",  OperationGt));
        lib.Add(make_binary_fn("lessequal",    OperationLe));
        lib.Add(make_binary_fn("greaterequal", OperationGe));

        // -- logical
        lib.Add(make_binary_fn("and", OperationAnd));
        lib.Add(make_binary_fn("or",  OperationOr));

        // -- bitwise
        lib.Add(make_binary_fn("bitand", OperationBitAnd));
        lib.Add(make_binary_fn("bitor",  OperationBitOr));
        lib.Add(make_binary_fn("bitxor", OperationBitXor));

        // -- shift
        lib.Add(make_binary_fn("shiftleft",  OperationShl));
        lib.Add(make_binary_fn("shiftright", OperationShr));

        // -- unary
        lib.Add(make_unary_fn("negate", OperationNegate));
        lib.Add(make_unary_fn("not",    OperationNot));
        lib.Add(make_unary_fn("bitnot", OperationBitNot));

        // -- ternary
        lib.Add(Function("conditional",
            std::vector<FunctionParam>{
                Param("condition"), Param("true_value"), Param("false_value")},
            [](const Function::ArgMap& args) {
                return OperationConditional(
                    args.at("condition"), args.at("true_value"), args.at("false_value"));
            }));

        // Binary math
        lib.Add(make_binary_fn("pow",   OperationPow));
        lib.Add(make_binary_fn("atan2", OperationAtan2));
        lib.Add(make_binary_fn("root",  OperationRoot));
        lib.Add(make_binary_fn("max2",  OperationMax2));
        lib.Add(make_binary_fn("min2",  OperationMin2));

        // Reduce operations (innermost-dimension reduction)
        lib.Add(make_unary_fn("min",  Min));
        lib.Add(make_unary_fn("max",  Max));
        lib.Add(make_unary_fn("sum",  Sum));
        lib.Add(make_unary_fn("mean", Mean));

        return lib;
    }

} // namespace math
} // namespace rel
