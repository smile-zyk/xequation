#include "measurement.h"
#include "unit.h"
#include "data_frame.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace xdataset
{

    // =========================================================================
    // MeasurementTypeVisitor -- extracts DataKind / DataType from a variant.
    // (internal: defined here, not in the public header)
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
    // Measurement -- metadata inference
    // =========================================================================

    void Measurement::infer_metadata()
    {
        MeasurementTypeVisitor v;
        boost::apply_visitor(v, storage_);
        data_type_ = v.dtype;

        // Derive shape by dispatching on (kind, dtype).
        shape_.clear();
        DataKind kind = v.kind;
        switch (kind)
        {
            case DataKind::kScalar:
                break;  // shape_ remains empty

            case DataKind::kVector:
                switch (data_type_)
                {
                    case DataType::kReal:    shape_.push_back(boost::get<VecXd>(storage_).size());            break;
                    case DataType::kInteger: shape_.push_back(boost::get<VecXi>(storage_).size());            break;
                    case DataType::kComplex: shape_.push_back(boost::get<VecXcd>(storage_).size());           break;
                    case DataType::kString:  shape_.push_back(boost::get<VecXs>(storage_).dimension(0)); break;
                    default: break;  // kBoolean is scalar-only
                }
                break;

            case DataKind::kMatrix:
                switch (data_type_)
                {
                    case DataType::kReal:
                    {
                        const auto& m = boost::get<MatXd>(storage_);
                        shape_.push_back(m.rows()); shape_.push_back(m.cols());
                        break;
                    }
                    case DataType::kInteger:
                    {
                        const auto& m = boost::get<MatXi>(storage_);
                        shape_.push_back(m.rows()); shape_.push_back(m.cols());
                        break;
                    }
                    case DataType::kComplex:
                    {
                        const auto& m = boost::get<MatXcd>(storage_);
                        shape_.push_back(m.rows()); shape_.push_back(m.cols());
                        break;
                    }
                    case DataType::kString:
                    {
                        const auto& t = boost::get<MatXs>(storage_);
                        shape_.push_back(t.dimension(0)); shape_.push_back(t.dimension(1));
                        break;
                    }
                    default: break;  // kBoolean is scalar-only
                }
                break;
        }
    }

    // =========================================================================
    // Measurement -- default ctor
    // =========================================================================

    Measurement::Measurement()
        : data_type_(DataType::kReal)
        , shape_()
        , storage_(0.0)
        , unit_()
    {
    }

    // =========================================================================
    // Measurement -- static factories
    // =========================================================================

    Measurement Measurement::Real(double value, const Unit& u)
    {
        Measurement m;
        m.storage_ = value;
        m.data_type_   = DataType::kReal;
        m.unit_        = u;
        return m;
    }

    Measurement Measurement::Integer(int value, const Unit& u)
    {
        Measurement m;
        m.storage_ = value;
        m.data_type_   = DataType::kInteger;
        m.unit_        = u;
        return m;
    }

    Measurement Measurement::Complex(std::complex<double> value, const Unit& u)
    {
        Measurement m;
        m.storage_ = value;
        m.data_type_   = DataType::kComplex;
        m.unit_        = u;
        return m;
    }

    Measurement Measurement::String(std::string value)
    {
        Measurement m;
        m.storage_ = value;
        m.data_type_   = DataType::kString;
        return m;
    }

    Measurement Measurement::Boolean(bool value)
    {
        Measurement m;
        m.storage_ = value;
        m.data_type_   = DataType::kBoolean;
        return m;
    }

    Measurement Measurement::Vector(VecXd v, const Unit& u)
    {
        Measurement m;
        const Index sz = v.size();
        m.storage_ = std::move(v);
        m.data_type_   = DataType::kReal;
        m.unit_        = u;
        m.shape_.push_back(sz);
        return m;
    }

    Measurement Measurement::Vector(VecXi v, const Unit& u)
    {
        Measurement m;
        const Index sz = v.size();
        m.storage_ = std::move(v);
        m.data_type_   = DataType::kInteger;
        m.unit_        = u;
        m.shape_.push_back(sz);
        return m;
    }

    Measurement Measurement::Vector(VecXcd v, const Unit& u)
    {
        Measurement m;
        const Index sz = v.size();
        m.storage_ = std::move(v);
        m.data_type_   = DataType::kComplex;
        m.unit_        = u;
        m.shape_.push_back(sz);
        return m;
    }

    Measurement Measurement::Vector(VecConstMap<double> v, const Unit& u)
    {
        return Measurement::Vector(VecXd(v), u);
    }

    Measurement Measurement::Vector(VecConstMap<int> v, const Unit& u)
    {
        return Measurement::Vector(VecXi(v), u);
    }

    Measurement Measurement::Vector(VecConstMap<std::complex<double> > v,
                                    const Unit& u)
    {
        return Measurement::Vector(VecXcd(v), u);
    }

    Measurement Measurement::Vector(const VecXs& v)
    {
        Measurement m;
        m.storage_ = v;
        m.data_type_   = DataType::kString;
        m.shape_.push_back(v.dimension(0));
        return m;
    }

    Measurement Measurement::Matrix(MatXd m, const Unit& u)
    {
        Measurement mm;
        const Index rows = m.rows();
        const Index cols = m.cols();
        mm.storage_ = std::move(m);
        mm.data_type_   = DataType::kReal;
        mm.unit_        = u;
        mm.shape_.push_back(rows);
        mm.shape_.push_back(cols);
        return mm;
    }

    Measurement Measurement::Matrix(MatXi m, const Unit& u)
    {
        Measurement mm;
        const Index rows = m.rows();
        const Index cols = m.cols();
        mm.storage_ = std::move(m);
        mm.data_type_   = DataType::kInteger;
        mm.unit_        = u;
        mm.shape_.push_back(rows);
        mm.shape_.push_back(cols);
        return mm;
    }

    Measurement Measurement::Matrix(MatXcd m, const Unit& u)
    {
        Measurement mm;
        const Index rows = m.rows();
        const Index cols = m.cols();
        mm.storage_ = std::move(m);
        mm.data_type_   = DataType::kComplex;
        mm.unit_        = u;
        mm.shape_.push_back(rows);
        mm.shape_.push_back(cols);
        return mm;
    }

    Measurement Measurement::Matrix(MatConstMap<double> m, const Unit& u)
    {
        return Measurement::Matrix(MatXd(m), u);
    }

    Measurement Measurement::Matrix(MatConstMap<int> m, const Unit& u)
    {
        return Measurement::Matrix(MatXi(m), u);
    }

    Measurement Measurement::Matrix(MatConstMap<std::complex<double> > m,
                                    const Unit& u)
    {
        return Measurement::Matrix(MatXcd(m), u);
    }

    Measurement Measurement::Matrix(const MatXs& m)
    {
        Measurement mm;
        mm.storage_ = m;
        mm.data_type_   = DataType::kString;
        mm.shape_.push_back(m.dimension(0));
        mm.shape_.push_back(m.dimension(1));
        return mm;
    }

    // =========================================================================
    // Measurement -- queries
    // =========================================================================

    bool Measurement::has_value() const
    {
        // Default-constructed Measurement is 0.0 real scalar -- always has a value.
        // (variant is never empty.)
        return true;
    }

    std::string Measurement::to_string() const
    {
        MeasurementFormatter fmt(unit_);
        return boost::apply_visitor(fmt, storage_);
    }

    // =========================================================================
    // Measurement -- element_at (vector -> scalar / matrix -> scalar)
    // =========================================================================

    Measurement Measurement::element_at(Index i) const
    {
        if (shape_.kind() != DataKind::kVector)
            throw std::logic_error("element_at(Index) requires vector data");
        switch (data_type_)
        {
            case DataType::kReal:    return Measurement::Real(boost::get<VecXd>(storage_)(i), unit_);
            case DataType::kInteger: return Measurement::Integer(boost::get<VecXi>(storage_)(i), unit_);
            case DataType::kComplex: return Measurement::Complex(boost::get<VecXcd>(storage_)(i), unit_);
            case DataType::kString:  return Measurement::String(boost::get<VecXs>(storage_)(i));
            default: break;  // kBoolean is scalar-only
        }
        throw std::logic_error("unsupported dtype");
    }

    Measurement Measurement::element_at(Index r, Index c) const
    {
        if (shape_.kind() != DataKind::kMatrix)
            throw std::logic_error("element_at(Index, Index) requires matrix data");
        switch (data_type_)
        {
            case DataType::kReal:    return Measurement::Real(boost::get<MatXd>(storage_)(r, c), unit_);
            case DataType::kInteger: return Measurement::Integer(boost::get<MatXi>(storage_)(r, c), unit_);
            case DataType::kComplex: return Measurement::Complex(boost::get<MatXcd>(storage_)(r, c), unit_);
            case DataType::kString:  return Measurement::String(boost::get<MatXs>(storage_)(r, c));
            default: break;  // kBoolean is scalar-only
        }
        throw std::logic_error("unsupported dtype");
    }

    // =========================================================================
    // Measurement -- at (slicing via MultiIndexSelector)
    // =========================================================================

    Measurement Measurement::at(const std::vector<MultiIndexSelector>& selectors) const
    {
        if (shape_.kind() == DataKind::kScalar)
            throw std::logic_error("at is invalid for scalar Measurement");

        const std::size_t ndim = (shape_.kind() == DataKind::kVector) ? 1 : 2;
        if (selectors.size() > ndim)
            throw std::invalid_argument("too many selectors for Measurement::at");

        // Pad short selectors with Any()
        std::vector<MultiIndexSelector> padded = selectors;
        while (padded.size() < ndim)
            padded.push_back(MultiIndexSelector::Any());

        if (shape_.kind() == DataKind::kVector)
        {
            const std::vector<Index> selected = padded[0].resolve(shape_[0]);

            if (selected.size() == 1)
                return element_at(selected[0]);

            // Sub-vector: extract selected elements
            switch (data_type_)
            {
                case DataType::kReal: {
                    const auto& src = boost::get<VecXd>(storage_);
                    VecXd dst(static_cast<Index>(selected.size()));
                    for (std::size_t i = 0; i < selected.size(); ++i)
                        dst(static_cast<Index>(i)) = src(selected[i]);
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kInteger: {
                    const auto& src = boost::get<VecXi>(storage_);
                    VecXi dst(static_cast<Index>(selected.size()));
                    for (std::size_t i = 0; i < selected.size(); ++i)
                        dst(static_cast<Index>(i)) = src(selected[i]);
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kComplex: {
                    const auto& src = boost::get<VecXcd>(storage_);
                    VecXcd dst(static_cast<Index>(selected.size()));
                    for (std::size_t i = 0; i < selected.size(); ++i)
                        dst(static_cast<Index>(i)) = src(selected[i]);
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kString: {
                    const auto& src = boost::get<VecXs>(storage_);
                    VecXs dst(static_cast<Index>(selected.size()));
                    for (std::size_t i = 0; i < selected.size(); ++i)
                        dst(static_cast<Index>(i)) = src(selected[i]);
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                default: break;
            }
            throw std::logic_error("unsupported dtype in Measurement::at");
        }

        // Matrix
        const std::vector<Index> sel_rows = padded[0].resolve(shape_[0]);
        const std::vector<Index> sel_cols = padded[1].resolve(shape_[1]);

        if (sel_rows.size() == 1 && sel_cols.size() == 1)
            return element_at(sel_rows[0], sel_cols[0]);

        if (sel_rows.size() == 1 || sel_cols.size() == 1)
        {
            // Single row or single column -> vector
            const bool single_row = (sel_rows.size() == 1);
            const std::vector<Index>& remaining = single_row ? sel_cols : sel_rows;
            const Index width = static_cast<Index>(remaining.size());

            switch (data_type_)
            {
                case DataType::kReal: {
                    const auto& src = boost::get<MatXd>(storage_);
                    VecXd dst(width);
                    for (Index i = 0; i < width; ++i) {
                        Index r = single_row ? sel_rows[0] : remaining[i];
                        Index c = single_row ? remaining[i] : sel_cols[0];
                        dst(i) = src(r, c);
                    }
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kInteger: {
                    const auto& src = boost::get<MatXi>(storage_);
                    VecXi dst(width);
                    for (Index i = 0; i < width; ++i) {
                        Index r = single_row ? sel_rows[0] : remaining[i];
                        Index c = single_row ? remaining[i] : sel_cols[0];
                        dst(i) = src(r, c);
                    }
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kComplex: {
                    const auto& src = boost::get<MatXcd>(storage_);
                    VecXcd dst(width);
                    for (Index i = 0; i < width; ++i) {
                        Index r = single_row ? sel_rows[0] : remaining[i];
                        Index c = single_row ? remaining[i] : sel_cols[0];
                        dst(i) = src(r, c);
                    }
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                case DataType::kString: {
                    const auto& src = boost::get<MatXs>(storage_);
                    VecXs dst(width);
                    for (Index i = 0; i < width; ++i) {
                        Index r = single_row ? sel_rows[0] : remaining[i];
                        Index c = single_row ? remaining[i] : sel_cols[0];
                        dst(i) = src(r, c);
                    }
                    return Measurement::Vector(dst).set_unit(unit_);
                }
                default: break;
            }
            throw std::logic_error("unsupported dtype in Measurement::at");
        }

        // Sub-matrix: extract selected rows x columns
        switch (data_type_)
        {
            case DataType::kReal: {
                const auto& src = boost::get<MatXd>(storage_);
                MatXd dst(static_cast<Index>(sel_rows.size()),
                          static_cast<Index>(sel_cols.size()));
                for (Index i = 0; i < static_cast<Index>(sel_rows.size()); ++i)
                    for (Index j = 0; j < static_cast<Index>(sel_cols.size()); ++j)
                        dst(i, j) = src(sel_rows[i], sel_cols[j]);
                return Measurement::Matrix(dst).set_unit(unit_);
            }
            case DataType::kInteger: {
                const auto& src = boost::get<MatXi>(storage_);
                MatXi dst(static_cast<Index>(sel_rows.size()),
                          static_cast<Index>(sel_cols.size()));
                for (Index i = 0; i < static_cast<Index>(sel_rows.size()); ++i)
                    for (Index j = 0; j < static_cast<Index>(sel_cols.size()); ++j)
                        dst(i, j) = src(sel_rows[i], sel_cols[j]);
                return Measurement::Matrix(dst).set_unit(unit_);
            }
            case DataType::kComplex: {
                const auto& src = boost::get<MatXcd>(storage_);
                MatXcd dst(static_cast<Index>(sel_rows.size()),
                           static_cast<Index>(sel_cols.size()));
                for (Index i = 0; i < static_cast<Index>(sel_rows.size()); ++i)
                    for (Index j = 0; j < static_cast<Index>(sel_cols.size()); ++j)
                        dst(i, j) = src(sel_rows[i], sel_cols[j]);
                return Measurement::Matrix(dst).set_unit(unit_);
            }
            case DataType::kString: {
                const auto& src = boost::get<MatXs>(storage_);
                MatXs dst(static_cast<Index>(sel_rows.size()),
                          static_cast<Index>(sel_cols.size()));
                for (Index i = 0; i < static_cast<Index>(sel_rows.size()); ++i)
                    for (Index j = 0; j < static_cast<Index>(sel_cols.size()); ++j)
                        dst(i, j) = src(sel_rows[i], sel_cols[j]);
                return Measurement::Matrix(dst).set_unit(unit_);
            }
            default: break;
        }
        throw std::logic_error("unsupported dtype in Measurement::at");
    }

    // =========================================================================
    // MeasurementFormatter
    // =========================================================================

    std::string MeasurementFormatter::with_unit(const std::string& s) const
    {
        if (!unit_.has_dimension())
            return s;
        return s + " " + unit_.to_string();
    }

    // --- scalar with auto-scale ------------------------------------------

    std::string MeasurementFormatter::operator()(double v) const
    {
        if (!std::isfinite(v)) return "<invalid>";
        auto bd = unit_.best_display(v);
        std::ostringstream oss;
        oss << (v * bd.scale);
        if (bd.name.empty())
            return oss.str();
        return oss.str() + " " + bd.name;
    }

    std::string MeasurementFormatter::operator()(int v) const
    {
        auto bd = unit_.best_display(static_cast<double>(v));
        std::ostringstream oss;
        oss << (v * bd.scale);
        if (bd.name.empty())
            return oss.str();
        return oss.str() + " " + bd.name;
    }

    std::string MeasurementFormatter::operator()(const std::complex<double>& v) const
    {
        if (!std::isfinite(v.real()) || !std::isfinite(v.imag())) return "<invalid>";
        auto bd = unit_.best_display(v.real());
        std::ostringstream oss;
        oss << (v.real() * bd.scale);
        double imag = v.imag() * bd.scale;
        if (imag >= 0.0) oss << "+";
        oss << imag << "i";
        if (bd.name.empty())
            return oss.str();
        return oss.str() + " " + bd.name;
    }

    std::string MeasurementFormatter::operator()(const std::string& v) const
    {
        return v;
    }

    std::string MeasurementFormatter::operator()(bool v) const
    {
        (void)unit_;   // boolean never carries a unit
        return v ? "TRUE" : "FALSE";
    }

    // -- vector --------------------------------------------------------------

    std::string MeasurementFormatter::operator()(const VecXd& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index i = 0; i < v.size(); ++i)
        {
            if (i > 0) oss << ",";
            if (!std::isfinite(v(i))) oss << "<invalid>";
            else oss << v(i);
        }
        oss << "]";
        return with_unit(oss.str());
    }

    std::string MeasurementFormatter::operator()(const VecXi& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index i = 0; i < v.size(); ++i)
        {
            if (i > 0) oss << ",";
            oss << v(i);
        }
        oss << "]";
        return with_unit(oss.str());
    }

    std::string MeasurementFormatter::operator()(const VecXcd& v) const
    {
        auto bd = unit_.best_display(v.size() > 0 ? v(0).real() : 0.0);
        std::ostringstream oss;
        oss << "[";
        for (Index i = 0; i < v.size(); ++i)
        {
            if (i > 0) oss << ",";
            if (!std::isfinite(v(i).real()) || !std::isfinite(v(i).imag())) {
                oss << "<invalid>";
            } else {
                double r = v(i).real() * bd.scale;
                double im = v(i).imag() * bd.scale;
                oss << r;
                if (im >= 0.0) oss << "+";
                oss << im << "i";
            }
        }
        oss << "]";
        if (bd.name.empty())
            return oss.str();
        return oss.str() + " " + bd.name;
    }

    std::string MeasurementFormatter::operator()(const VecXs& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index i = 0; i < v.dimension(0); ++i)
        {
            if (i > 0) oss << ",";
            oss << v(i);
        }
        oss << "]";
        return oss.str();
    }

    // -- matrix --------------------------------------------------------------

    std::string MeasurementFormatter::operator()(const MatXd& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index r = 0; r < v.rows(); ++r)
        {
            if (r > 0) oss << ",";
            oss << "[";
            for (Index c = 0; c < v.cols(); ++c)
            {
                if (c > 0) oss << ",";
                if (!std::isfinite(v(r, c))) oss << "<invalid>";
                else oss << v(r, c);
            }
            oss << "]";
        }
        oss << "]";
        return with_unit(oss.str());
    }

    std::string MeasurementFormatter::operator()(const MatXi& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index r = 0; r < v.rows(); ++r)
        {
            if (r > 0) oss << ",";
            oss << "[";
            for (Index c = 0; c < v.cols(); ++c)
            {
                if (c > 0) oss << ",";
                oss << v(r, c);
            }
            oss << "]";
        }
        oss << "]";
        return with_unit(oss.str());
    }

    std::string MeasurementFormatter::operator()(const MatXcd& v) const
    {
        auto bd = unit_.best_display(v.size() > 0 ? v(0, 0).real() : 0.0);
        std::ostringstream oss;
        oss << "[";
        for (Index r = 0; r < v.rows(); ++r)
        {
            if (r > 0) oss << ",";
            oss << "[";
            for (Index c = 0; c < v.cols(); ++c)
            {
                if (c > 0) oss << ",";
                if (!std::isfinite(v(r, c).real()) || !std::isfinite(v(r, c).imag())) {
                    oss << "<invalid>";
                } else {
                    double rv = v(r, c).real() * bd.scale;
                    double im = v(r, c).imag() * bd.scale;
                    oss << rv;
                    if (im >= 0.0) oss << "+";
                    oss << im << "i";
                }
            }
            oss << "]";
        }
        oss << "]";
        if (bd.name.empty())
            return oss.str();
        return oss.str() + " " + bd.name;
    }

    std::string MeasurementFormatter::operator()(const MatXs& v) const
    {
        std::ostringstream oss;
        oss << "[";
        for (Index r = 0; r < v.dimension(0); ++r)
        {
            if (r > 0) oss << ",";
            oss << "[";
            for (Index c = 0; c < v.dimension(1); ++c)
            {
                if (c > 0) oss << ",";
                oss << v(r, c);
            }
            oss << "]";
        }
        oss << "]";
        return oss.str();
    }

// =========================================================================
//  Measurement -> canonicalized
// =========================================================================

Measurement Measurement::canonicalized() const {
    if (data_type_ == DataType::kString ||
        data_type_ == DataType::kBoolean) {
        Measurement result(*this);
        result.unit_ = unit_.canonicalized();
        return result;
    }

    double mult = unit_.multiplier();
    Unit target = unit_.canonicalized();

    // Fast path: already canonical
    if (mult == 1.0) {
        Measurement result(*this);
        result.unit_ = target;
        return result;
    }

    // Promote integer to real when scaling is needed
    DataType res_dtype = (data_type_ == DataType::kInteger) ? DataType::kReal : data_type_;

    Measurement result;
    result.data_type_ = res_dtype;
    result.shape_ = shape_;
    result.unit_  = target;

    Index count = 1;
    if (shape_.kind() == DataKind::kVector) count = shape_[0];
    else if (shape_.kind() == DataKind::kMatrix) count = shape_[0] * shape_[1];

    if (shape_.kind() == DataKind::kScalar) {
        double v = (data_type_ == DataType::kInteger)
                   ? static_cast<double>(boost::get<int>(storage_))
                   : boost::get<double>(storage_);
        v *= mult;
        if (res_dtype == DataType::kComplex) {
            std::complex<double> cv = boost::get<std::complex<double> >(storage_);
            cv *= mult;
            result.storage_ = cv;
        } else {
            result.storage_ = v;
        }
        return result;
    }

    if (shape_.kind() == DataKind::kVector) {
        if (res_dtype == DataType::kComplex) {
            VecXcd vec = boost::get<VecXcd>(storage_);
            vec *= mult;
            result.storage_ = vec;
        } else {
            VecXd vec(count);
            for (Index i = 0; i < count; ++i) {
                double v = (data_type_ == DataType::kInteger)
                           ? static_cast<double>(boost::get<VecXi>(storage_)(i))
                           : boost::get<VecXd>(storage_)(i);
                vec(i) = v * mult;
            }
            result.storage_ = vec;
        }
        return result;
    }

    // Matrix
    if (res_dtype == DataType::kComplex) {
        MatXcd mat = boost::get<MatXcd>(storage_);
        mat *= mult;
        result.storage_ = mat;
    } else {
        Index rows = shape_[0], cols = shape_[1];
        MatXd mat(rows, cols);
        for (Index r = 0; r < rows; ++r) {
            for (Index c = 0; c < cols; ++c) {
                double v = (data_type_ == DataType::kInteger)
                           ? static_cast<double>(boost::get<MatXi>(storage_)(r, c))
                           : boost::get<MatXd>(storage_)(r, c);
                mat(r, c) = v * mult;
            }
        }
        result.storage_ = mat;
    }
    return result;
}

bool Measurement::is_canonicalized() const {
    return unit_.is_canonical();
}

// =========================================================================
// Measurement::to_dataframe
// =========================================================================

std::unique_ptr<DataFrame> Measurement::to_dataframe(
    const std::string& name) const
{
    return DataFrame::FromMeasurement(*this, name);
}

} // namespace xdataset
