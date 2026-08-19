#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <Eigen/Dense>
#include <unsupported/Eigen/CXX11/Tensor>

#include <boost/variant.hpp>

#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

#include "xdataset_predefine.h"
#include "data_shape.h"
#include "unit.h"
#include "multi_index_selector.h"

namespace xdataset
{

    class MeasurementDataFrame;

    // =========================================================================
    // Measurement -- a single named value with units (scalar | vector | matrix)
    // =========================================================================
    //
    // Measurement is the "datum" of the xdataset type system.  It is a
    // stack-friendly value type that can represent a 0-d scalar, 1-d vector
    // or 2-d matrix in any of the four supported dtypes (real, integer,
    // complex, string) and always carries a physical Unit.
    //
    // Relationship to other types:
    //   - DataSeries stores many Measurements *contiguously in memory*.
    //     Measurement is obtained via DataSeries::measurement_at(row) and serves as
    //     the user-facing value type.
    //   - DataFrame (the tabular CSV view) stores Measurement directly in
    //     DataFrameRow::fields as the cell type.
    //   - DataArray wraps a DataSeries with coordinate axes and can
    //     interact arithmetically with Measurement (future).
    //
    // Internals:
    //   - Storage is a boost::variant over all 12 (DataKind x DataType)
    //     concrete types.  The variant itself lives on the stack; Eigen
    //     dynamic types allocate their element buffers on the heap, but
    //     this is expected to be modest for a single measurement.
    //   - DataKind, DataType, and shape are derived from the active
    //     variant alternative at construction time and cached.
    // =========================================================================

    class XDATASET_API Measurement
    {
    public:
        // ----------------------------------
        // Storage variant -- one alternative per (DataKind, DataType)
        // ----------------------------------
        using Storage = boost::variant<
            // --- DataKind::kScalar ---
            double,
            int,
            std::complex<double>,
            std::string,
            bool,                           // DataType::kBoolean

            // --- DataKind::kVector ---
            VecXd,                          // DataType::kReal     (1 行, w 列, RowMajor)
            VecXi,                          // DataType::kInteger
            VecXcd,                         // DataType::kComplex
            VecXs,                          // DataType::kString

            // --- DataKind::kMatrix ---
            MatXd,                          // DataType::kReal    (RowMajor)
            MatXi,                          // DataType::kInteger
            MatXcd,                         // DataType::kComplex
            MatXs                           // DataType::kString
        >;

        // ======== construction ==============================================

        /// Default: kReal scalar 0.0, dimensionless unit.
        Measurement();

        // Copy / move (compiler-generated is fine -- variant is deep-copyable)
        Measurement(const Measurement&) = default;
        Measurement& operator=(const Measurement&) = default;
        Measurement(Measurement&&) = default;
        Measurement& operator=(Measurement&&) = default;

        // ======== static factories ==========================================

        /// @{
        /// Scalar factories (0-d).  Real / Integer / Complex carry an optional
        /// unit (default: dimensionless).  Boolean and String values cannot
        /// carry a physical unit, so their factories take no unit.
        static Measurement Real(double value, const Unit& u = Unit());
        static Measurement Integer(int value, const Unit& u = Unit());
        static Measurement Complex(std::complex<double> value, const Unit& u = Unit());
        static Measurement String(std::string value);
        static Measurement Boolean(bool value);
        /// @}

        /// @{
        /// Vector factories (1-d) -- numeric (行向量), with optional unit.
        /// By-value parameters: lvalues copy, rvalues move (no extra copy for
        /// `Measurement::Vector(MatXi(...))` style temporaries).
        static Measurement Vector(VecXd v, const Unit& u = Unit());
        static Measurement Vector(VecXi v, const Unit& u = Unit());
        static Measurement Vector(VecXcd v, const Unit& u = Unit());
        /// @}
        /// Vector factory from an Eigen Map view (e.g. DataSeries::vector_at).
        /// Copies the viewed data into a standalone Measurement.
        static Measurement Vector(VecConstMap<double> v, const Unit& u = Unit());
        static Measurement Vector(VecConstMap<int> v, const Unit& u = Unit());
        static Measurement Vector(VecConstMap<std::complex<double> > v,
                                  const Unit& u = Unit());
        static Measurement Vector(const VecXs& v);   // string rows: no unit

        /// @{
        /// Scalar factory dispatching on the element type T.  Used internally
        /// by transform() when the callback returns a scalar of an arbitrary
        /// type; the appropriate Real / Integer / Complex factory is selected
        /// at compile time.  String outputs carry no unit.
        template <typename T>
        static Measurement Scalar(const T& v, const Unit& u = Unit());
        /// @}

        /// @{
        /// Matrix factories (2-d) -- numeric (RowMajor), with optional unit.
        /// By-value parameters: lvalues copy, rvalues move.
        static Measurement Matrix(MatXd m, const Unit& u = Unit());
        static Measurement Matrix(MatXi m, const Unit& u = Unit());
        static Measurement Matrix(MatXcd m, const Unit& u = Unit());
        /// @}
        /// Matrix factory from an Eigen Map view (e.g. DataSeries::matrix_at).
        /// Copies the viewed data into a standalone Measurement.
        static Measurement Matrix(MatConstMap<double> m, const Unit& u = Unit());
        static Measurement Matrix(MatConstMap<int> m, const Unit& u = Unit());
        static Measurement Matrix(MatConstMap<std::complex<double> > m,
                                  const Unit& u = Unit());
        static Measurement Matrix(const MatXs& m);   // string cells: no unit

        // ======== metadata queries ==========================================

        DataKind data_kind() const { return shape_.kind(); }
        DataType data_type() const { return data_type_; }
        const DataShape& shape() const { return shape_; }
        const Unit& unit() const { return unit_; }

        /// Number of elements in one cell: scalar=1, vector=shape[0], matrix=shape[0]*shape[1]
        Index element_count() const { return shape_.element_count(); }

        Measurement& set_unit(const Unit& u) {
            if (data_type_ == DataType::kBoolean) {
                if (u.has_dimension())
                    throw std::invalid_argument("Boolean measurements cannot have a unit");
            }
            unit_ = u;
            return *this;
        }

        /// True when the stored value is not the default-constructed zero.
        bool has_value() const;

        // ======== raw storage access ========================================

        const Storage& storage() const { return storage_; }

        // ======== typed accessors ===========================================

        /// @{
        /// Scalar access -- throws std::bad_cast if T doesn't match dtype.
        template <typename T> T as_scalar() const;
        /// @}

        /// @{
        /// Vector access (returns Eigen Map for numeric, ref for string tensor).
        template <typename T>
        typename std::enable_if<
            !std::is_same<T, std::string>::value,
            VecConstMap<T>>::type
        as_vector() const;

        template <typename T>
        typename std::enable_if<
            std::is_same<T, std::string>::value,
            const VecXs&>::type
        as_vector() const;
        /// @}

        /// @{
        /// Matrix access (returns Eigen Map for numeric, ref for string tensor).
        template <typename T>
        typename std::enable_if<
            !std::is_same<T, std::string>::value,
            MatConstMap<T>>::type
        as_matrix() const;

        template <typename T>
        typename std::enable_if<
            std::is_same<T, std::string>::value,
            const MatXs&>::type
        as_matrix() const;
        /// @}

        // ======== element access (vector / matrix -- scalar) =================

        /// Return the i-th element as a scalar Measurement (preserves unit).
        Measurement element_at(Index i) const;

        /// Return the (r, c)-th element as a scalar Measurement (preserves unit).
        Measurement element_at(Index r, Index c) const;

        /// Return a sub-Measurement selected by MultiIndexSelectors.
        /// For vectors: 1 selector → scalar (if single) or sub-vector.
        /// For matrices: 2 selectors → scalar/vector/sub-matrix.
        /// Preserves unit.  Not valid for scalar data.
        Measurement at(const std::vector<MultiIndexSelector>& selectors) const;

        // ======== per-element transform ====================================

        /// Apply a function to each scalar element of this Measurement.
        /// Shape, kind, and unit are preserved.  The output dtype is deduced
        /// from the return type of `func`, so it may differ from the input
        /// dtype (e.g. complex → real for abs, double → int for round).
        ///
        /// For Scalars: func is called once on the scalar value.
        /// For Vectors: func is called on each element of the vector.
        /// For Matrices: func is called on each element of the matrix.
        ///
        /// The function must be callable with the Measurement's scalar type
        /// (double, int, std::complex<double>, or std::string).  Passing a
        /// function whose argument type does not match the Measurement's
        /// data type is a compile error.
        ///
        /// Example:
        ///   auto v = Value::Vector(VecXd{1.0, 2.0, 3.0});
        ///   Measurement sq = v.as_measurement().transform([](double x) { return x * x; });
        template <typename Func>
        Measurement transform(Func&& func) const
        {
            switch (shape_.kind()) {
                case DataKind::kScalar:
                    return transform_scalar_dispatch(std::forward<Func>(func));
                case DataKind::kVector:
                    return transform_vector_dispatch(std::forward<Func>(func));
                case DataKind::kMatrix:
                    return transform_matrix_dispatch(std::forward<Func>(func));
            }
            return *this;
        }

        // ======== formatting ================================================

        /// Return a human-readable string representation.
        std::string to_string() const;

        // ======== DataFrame conversion ======================================

        /// Create a MeasurementDataFrame with this measurement as the single
        /// row, using \p name as the column header prefix.
        MeasurementDataFrame to_dataframe(const std::string& name) const;

        // ======== canonicalisation ======================================

        /// Return a canonicalised copy (value scaled to SI, unit = base_units).
        Measurement canonicalized() const;

        /// True when the stored unit is already canonical (multiplier == 1, non-affine).
        bool is_canonicalized() const;

    private:
        void infer_metadata();


        // ---- transform helpers ------------------------------------------
        //
        //  Each dispatch switches on data_type_ and calls the typed impl.
        //  All switch alternatives are instantiated at compile time;
        //  the int/long overload trick picks the int overload when Func(T)
        //  is valid (SFINAE), otherwise the long overload silently returns
        //  *this.  At runtime only the matching data_type_ branch executes.

        // Scalar dispatch
        template <typename Func>
        Measurement transform_scalar_dispatch(Func&& func) const {
            switch (data_type_) {
                case DataType::kReal:
                    return transform_scalar_impl<double>(std::forward<Func>(func), 0);
                case DataType::kInteger:
                    return transform_scalar_impl<int>(std::forward<Func>(func), 0);
                case DataType::kComplex:
                    return transform_scalar_impl<std::complex<double>>(std::forward<Func>(func), 0);
                case DataType::kString:
                    return transform_scalar_impl<std::string>(std::forward<Func>(func), 0);
                default:
                    return *this;
            }
        }

        // Vector dispatch
        template <typename Func>
        Measurement transform_vector_dispatch(Func&& func) const {
            switch (data_type_) {
                case DataType::kReal:
                    return transform_vector_impl<double>(std::forward<Func>(func), 0);
                case DataType::kInteger:
                    return transform_vector_impl<int>(std::forward<Func>(func), 0);
                case DataType::kComplex:
                    return transform_vector_impl<std::complex<double>>(std::forward<Func>(func), 0);
                default:
                    return *this;
            }
        }

        // Matrix dispatch
        template <typename Func>
        Measurement transform_matrix_dispatch(Func&& func) const {
            switch (data_type_) {
                case DataType::kReal:
                    return transform_matrix_impl<double>(std::forward<Func>(func), 0);
                case DataType::kInteger:
                    return transform_matrix_impl<int>(std::forward<Func>(func), 0);
                case DataType::kComplex:
                    return transform_matrix_impl<std::complex<double>>(std::forward<Func>(func), 0);
                default:
                    return *this;
            }
        }

        // -- SFINAE scalar (int priority) ---------------------------------
        //  Uses expression SFINAE on a non-type template parameter so that
        //  Measurement does not appear inside a decltype within the class
        //  body (avoids IntelliSense "incomplete type" false positives).

        template <typename T, typename Func,
            decltype(std::declval<Func>()(std::declval<const T&>()), 0) = 0>
        Measurement transform_scalar_impl(Func&& func, int) const
        {
            return Scalar(func(boost::get<T>(storage_)), unit_);
        }

        template <typename T, typename Func>
        Measurement transform_scalar_impl(Func&& func, long) const {
            (void)func; return *this;
        }

        // -- SFINAE vector (int priority) ---------------------------------

        template <typename T, typename Func,
            decltype(std::declval<Func>()(std::declval<const T&>()), 0) = 0>
        Measurement transform_vector_impl(Func&& func, int) const
        {
            typedef decltype(func(std::declval<const T&>())) Out;
            Index w = shape_[0];
            Eigen::Matrix<Out, 1, Eigen::Dynamic> v(w);
            const auto& src = boost::get<Eigen::Matrix<T, 1, Eigen::Dynamic>>(storage_);
            for (Index i = 0; i < w; ++i) v(i) = func(src(i));
            return Measurement::Vector(v, unit_);
        }

        template <typename T, typename Func>
        Measurement transform_vector_impl(Func&& func, long) const {
            (void)func; return *this;
        }

        // -- SFINAE matrix (int priority) ---------------------------------

        template <typename T, typename Func,
            decltype(std::declval<Func>()(std::declval<const T&>()), 0) = 0>
        Measurement transform_matrix_impl(Func&& func, int) const
        {
            typedef decltype(func(std::declval<const T&>())) Out;
            Index r = shape_[0], c = shape_[1];
            Eigen::Matrix<Out, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> m(r, c);
            const auto& src = boost::get<
                Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(storage_);
            for (Index i = 0; i < r; ++i)
                for (Index j = 0; j < c; ++j)
                    m(i, j) = func(src(i, j));
            return Measurement::Matrix(m, unit_);
        }

        template <typename T, typename Func>
        Measurement transform_matrix_impl(Func&& func, long) const {
            (void)func; return *this;
        }


        DataType             data_type_;
        DataShape            shape_;
        Storage              storage_;
        Unit                 unit_;
    };

    // =========================================================================
    // MeasurementFormatter -- boost::static_visitor that renders any stored
    //                         alternative to a human-readable string.
    // =========================================================================

    struct XDATASET_API MeasurementFormatter : public boost::static_visitor<std::string>
    {
        MeasurementFormatter() = default;
        explicit MeasurementFormatter(Unit u) : unit_(std::move(u)) {}

        std::string operator()(double v) const;
        std::string operator()(int v) const;
        std::string operator()(const std::complex<double>& v) const;
        std::string operator()(const std::string& v) const;
        std::string operator()(bool v) const;

        std::string operator()(const VecXd& v) const;
        std::string operator()(const VecXi& v) const;
        std::string operator()(const VecXcd& v) const;
        std::string operator()(const VecXs& v) const;

        std::string operator()(const MatXd& v) const;
        std::string operator()(const MatXi& v) const;
        std::string operator()(const MatXcd& v) const;
        std::string operator()(const MatXs& v) const;

    private:
        std::string with_unit(const std::string& s) const;

        Unit unit_;
    };

    // =========================================================================
    // MeasurementTypeVisitor -- extracts DataKind / DataType from a variant.
    // =========================================================================

    struct MeasurementTypeVisitor : public boost::static_visitor<void>
    {
        DataKind kind   = DataKind::kScalar;
        DataType dtype  = DataType::kReal;

        void operator()(double)                    { kind = DataKind::kScalar;  dtype = DataType::kReal;    }
        void operator()(int)                       { kind = DataKind::kScalar;  dtype = DataType::kInteger; }
        void operator()(const std::complex<double>&){ kind = DataKind::kScalar;  dtype = DataType::kComplex; }
        void operator()(const std::string&)         { kind = DataKind::kScalar;  dtype = DataType::kString;  }
        void operator()(bool)                       { kind = DataKind::kScalar;  dtype = DataType::kBoolean; }

        void operator()(const VecXd&)           { kind = DataKind::kVector; dtype = DataType::kReal;    }
        void operator()(const VecXi&)           { kind = DataKind::kVector; dtype = DataType::kInteger; }
        void operator()(const VecXcd&)          { kind = DataKind::kVector; dtype = DataType::kComplex; }
        void operator()(const VecXs&)           { kind = DataKind::kVector; dtype = DataType::kString;  }

        void operator()(const MatXd&)           { kind = DataKind::kMatrix; dtype = DataType::kReal;    }
        void operator()(const MatXi&)           { kind = DataKind::kMatrix; dtype = DataType::kInteger; }
        void operator()(const MatXcd&)          { kind = DataKind::kMatrix; dtype = DataType::kComplex; }
        void operator()(const MatXs&)           { kind = DataKind::kMatrix; dtype = DataType::kString;  }
    };

    // =========================================================================
    // Template implementation
    // =========================================================================

    // -- as_scalar<T> ------------------------------------------------------------

    template <typename T>
    T Measurement::as_scalar() const
    {
        if (shape_.kind() != DataKind::kScalar)
            throw std::logic_error("as_scalar: Measurement is not a scalar (kind=" +
                std::to_string(static_cast<int>(shape_.kind())) + ")");
        return boost::get<T>(storage_);
    }

    // -- as_vector<T> (numeric) --------------------------------------------------

    template <typename T>
    typename std::enable_if<
        !std::is_same<T, std::string>::value,
        VecConstMap<T>>::type
    Measurement::as_vector() const
    {
        if (shape_.kind() != DataKind::kVector)
            throw std::logic_error("as_vector: Measurement is not a vector (kind=" +
                std::to_string(static_cast<int>(shape_.kind())) + ")");
        typedef Vec<T> VecType;
        const VecType& vec = boost::get<VecType>(storage_);
        return VecConstMap<T>(vec.data(), 1, vec.size());
    }

    // -- as_vector<T> (string) ---------------------------------------------------

    template <typename T>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
        const VecXs&>::type
    Measurement::as_vector() const
    {
        if (shape_.kind() != DataKind::kVector)
            throw std::logic_error("as_vector: Measurement is not a vector (kind=" +
                std::to_string(static_cast<int>(shape_.kind())) + ")");
        return boost::get<VecXs>(storage_);
    }

    // -- as_matrix<T> (numeric) --------------------------------------------------

    template <typename T>
    typename std::enable_if<
        !std::is_same<T, std::string>::value,
        MatConstMap<T>>::type
    Measurement::as_matrix() const
    {
        if (shape_.kind() != DataKind::kMatrix)
            throw std::logic_error("as_matrix: Measurement is not a matrix (kind=" +
                std::to_string(static_cast<int>(shape_.kind())) + ")");
        typedef Mat<T> MatType;
        const MatType& mat = boost::get<MatType>(storage_);
        return MatConstMap<T>(mat.data(), mat.rows(), mat.cols());
    }

    // -- as_matrix<T> (string) ---------------------------------------------------

    template <typename T>
    typename std::enable_if<
        std::is_same<T, std::string>::value,
        const MatXs&>::type
    Measurement::as_matrix() const
    {
        if (shape_.kind() != DataKind::kMatrix)
            throw std::logic_error("as_matrix: Measurement is not a matrix (kind=" +
                std::to_string(static_cast<int>(shape_.kind())) + ")");
        return boost::get<MatXs>(storage_);
    }

    // =========================================================================
    //  Measurement::Scalar -- scalar factory dispatch by element type
    // =========================================================================

    template <>
    inline Measurement Measurement::Scalar<double>(const double& v,
                                                   const Unit& u)
    {
        return Measurement::Real(v, u);
    }

    template <>
    inline Measurement Measurement::Scalar<int>(const int& v,
                                                const Unit& u)
    {
        return Measurement::Integer(v, u);
    }

    template <>
    inline Measurement Measurement::Scalar<std::complex<double> >(
        const std::complex<double>& v, const Unit& u)
    {
        return Measurement::Complex(v, u);
    }

    template <>
    inline Measurement Measurement::Scalar<std::string>(
        const std::string& v, const Unit&)
    {
        return Measurement::String(v);   // strings carry no unit
    }

} // namespace xdataset

#endif // MEASUREMENT_H
