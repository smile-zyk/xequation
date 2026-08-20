// =============================================================================
//  xdataset -- Value implementation
// =============================================================================

#include "value.h"

#include "data_frame.h"  // DataFrame::FromDataArray
#include "data_series.h"

#include <stdexcept>

namespace rel {

using namespace xdataset;

// =========================================================================
//  Value
// =========================================================================

Value::Value() : storage_(Measurement()) {}

Value::Value(Measurement m) : storage_(std::move(m)) {}

Value::Value(const DataArray& da)
    : storage_(std::make_shared<DataArray>(da)) {}

Value::Value(std::shared_ptr<DataArray> da) : storage_(std::move(da)) {}

// ---- type queries ----------------------------------------------------------

bool Value::is_measurement() const {
    return storage_.which() == 0;
}

bool Value::is_data_array() const {
    return storage_.which() == 1;
}

// ---- accessors -------------------------------------------------------------

Measurement& Value::as_measurement() {
    return boost::get<Measurement>(storage_);
}

const Measurement& Value::as_measurement() const {
    return boost::get<Measurement>(storage_);
}

DataArray& Value::as_data_array() {
    return *boost::get<std::shared_ptr<DataArray>>(storage_);
}

const DataArray& Value::as_data_array() const {
    return *boost::get<std::shared_ptr<DataArray>>(storage_);
}

// ---- unified metadata ------------------------------------------------------

DataKind Value::data_kind() const {
    if (is_measurement()) return as_measurement().data_kind();
    return as_data_array().data().data_kind();
}

DataType Value::data_type() const {
    if (is_measurement()) return as_measurement().data_type();
    return as_data_array().data().data_type();
}

DataShape Value::data_shape() const {
    if (is_measurement()) return as_measurement().shape();
    return as_data_array().data().data_shape();
}

const Unit& Value::unit() const {
    if (is_measurement()) return as_measurement().unit();
    return as_data_array().data().unit();
}

Index Value::rows() const {
    if (is_measurement()) return 1;
    return static_cast<Index>(as_data_array().data().size());
}

Index Value::element_count() const {
    if (is_measurement()) return as_measurement().element_count();
    return as_data_array().element_count();
}

// ---- unified inspection ----------------------------------------------------

std::vector<std::string> Value::indep_names() const {
    if (is_measurement()) return {};
    return as_data_array().indep_names();
}

bool Value::is_dependent() const {
    if (is_measurement()) return false;
    return as_data_array().data_kind() == DataArrayKind::kDependent;
}

MultiDimensionSpec Value::dimension_spec() const {
    if (is_measurement()) {
        MultiDimensionSpec spec;
        spec.add_regular(1);
        return spec;
    }
    return as_data_array().multi_dimension_spec();
}

// ---- data / indep_data access ----------------------------------------------

DataSeries Value::data() const {
    if (is_measurement()) {
        const Measurement& m = as_measurement();
        DataSeries ds(m.data_type(), m.shape());
        ds.set_unit(m.unit());
        ds.append(m);
        return ds;
    }
    return as_data_array().data();
}

DataSeries Value::indep_data(Index index) const {
    if (is_measurement()) {
        if (index == 1)
            return DataSeries::CreateScalar<int>(1, Unit(), 0);
        throw std::out_of_range("Measurement only has indep index 1");
    }
    return as_data_array().indep_data(index);
}

DataSeries Value::indep_data(const std::string& name) const {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call indep_data");
    return as_data_array().indep_data(name);
}

Value Value::indep(Index index) const {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call indep");
    return Value(as_data_array().indep(index));
}

Value Value::indep(const std::string& name) const {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call indep");
    return Value(as_data_array().indep(name));
}

// ---- leaf / group iteration --------------------------------------------------

void Value::for_each_indep_group(
    Index indep_index,
    const MultiDimensionSpec::DimGroupVisitor& visitor) const
{
    if (is_measurement()) {
        if (indep_index == 1) {
            MultiDimensionSpec::DimGroup g;
            g.flat_start = 0;
            g.flat_end   = 1;
            g.multi_index = {0};
            visitor(g);
            return;
        }
        throw std::out_of_range("Measurement only has indep index 1");
    }
    as_data_array().for_each_indep_group(indep_index, visitor);
}

void Value::for_each_leaf_row(
    const MultiDimensionSpec::LeafRowVisitor& visitor) const
{
    if (is_measurement()) {
        MultiDimensionSpec::LeafRow leaf;
        leaf.row_flat = 0;
        leaf.multi_index = {0};
        leaf.dimension_row_indices = {0};
        visitor(leaf);
        return;
    }
    as_data_array().for_each_leaf_row(visitor);
}

void Value::for_each_leaf_row(
    const MultiDimensionSpec::LeafRowVisitor& visitor,
    Index start_flat_row, Index end_flat_row) const
{
    if (is_measurement()) {
        if (start_flat_row <= 0 && end_flat_row > 0) {
            MultiDimensionSpec::LeafRow leaf;
            leaf.row_flat = 0;
            leaf.multi_index = {0};
            leaf.dimension_row_indices = {0};
            visitor(leaf);
        }
        return;
    }
    as_data_array().for_each_leaf_row(visitor, start_flat_row, end_flat_row);
}

// ---- setters ----------------------------------------------------------------

void Value::set_data(Measurement value) {
    if (is_measurement()) {
        storage_ = std::move(value);
        return;
    }
    DataSeries ds(value.data_type(), value.shape());
    ds.append(value);
    as_data_array().set_data(std::move(ds));
}

void Value::set_data(DataSeries new_self) {
    if (is_measurement()) {
        if (new_self.size() == 0) return;
        storage_ = new_self.measurement_at(0);
        return;
    }
    as_data_array().set_data(std::move(new_self));
}

void Value::set_data(Index row, Measurement value) {
    if (is_measurement()) {
        if (row != 0)
            throw std::out_of_range("Measurement only has row 0");
        storage_ = std::move(value);
        return;
    }
    as_data_array().set_data(row, std::move(value));
}

void Value::set_indep_data(DataSeries new_series) {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call set_indep_data");
    as_data_array().set_indep_data(std::move(new_series));
}

void Value::set_indep_data(Index indep_index, DataSeries new_series) {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call set_indep_data");
    as_data_array().set_indep_data(indep_index, std::move(new_series));
}

void Value::set_indep_data(const std::string& indep_name, DataSeries new_series) {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call set_indep_data");
    as_data_array().set_indep_data(indep_name, std::move(new_series));
}

void Value::set_indep_data(Index indep_index, Index row, Measurement value) {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call set_indep_data");
    as_data_array().set_indep_data(indep_index, row, std::move(value));
}

void Value::set_indep_data(const std::string& indep_name, Index row, Measurement value) {
    if (is_measurement())
        throw std::runtime_error("Measurement cannot call set_indep_data");
    as_data_array().set_indep_data(indep_name, row, std::move(value));
}

Value Value::clone() const {
    if (is_measurement()) return *this;
    return Value(std::make_shared<DataArray>(as_data_array().clone()));
}

// ---- canonicalization ------------------------------------------------------

Value Value::canonicalized() const
{
    if (is_measurement()) {
        const Measurement& m = as_measurement();
        if (m.is_canonicalized()) return *this;
        return Value(m.canonicalized());
    }
    if (is_data_array()) {
        const DataArray& da = as_data_array();
        if (da.data().is_canonicalized()) return *this;

        auto canonical_datas = da.datas();
        canonical_datas[DataArray::kSelf] = da.data().canonicalized();

        DataArrayCreateInfo info;
        info.datas                = std::move(canonical_datas);
        info.multi_dimension_spec = da.multi_dimension_spec();
        info.kind                 = da.data_kind();

        return Value(std::make_shared<DataArray>(std::move(info)));
    }
    return *this;
}

bool Value::is_canonicalized() const
{
    if (is_measurement()) return as_measurement().is_canonicalized();
    if (is_data_array()) return as_data_array().data().is_canonicalized();
    return true;
}

// ---- formatting ------------------------------------------------------------

std::unique_ptr<xdataset::DataFrame> Value::data_frame(
    const std::string& name) const
{
    if (is_measurement())
    {
        return as_measurement().to_dataframe(name);
    }

    // DataArray: render with custom or default variable name.
    const xdataset::DataArray& da = as_data_array();
    const std::string& header = name.empty() ? "data" : name;
    return xdataset::DataFrame::FromDataArray(da, header);
}

std::string Value::to_string() const
{
    if (is_measurement())
    {
        // Inline compact form: reuses Measurement::to_string() with
        // auto-scaled units (e.g. "3.14 GHz", "[1, 2, 3]").
        return as_measurement().to_string();
    }

    // DataArray: tabular render is the natural compact representation.
    // (DataFrame::to_string leads with a newline, so the table never
    // glues onto preceding output.)
    return as_data_array().GetOrCreateDataFrame("data").to_string();
}

std::string Value::Format(const std::string& name, int max_rows) const
{
    // Both branches render a DataFrame table; DataFrame::to_string leads
    // with a newline so the table never glues onto preceding output.
    if (is_measurement())
    {
        const xdataset::Measurement& m = as_measurement();
        return m.to_dataframe(name)->to_string(max_rows);
    }

    // DataArray: render with custom or default variable name
    const xdataset::DataArray& da = as_data_array();
    const std::string& header = name.empty() ? "data" : name;
    return da.GetOrCreateDataFrame(header).to_string(max_rows);
}

// ---- convenience factories -------------------------------------------------

Value Value::Real(double v, const Unit& u) {
    return Value(Measurement::Real(v, u));
}

Value Value::Integer(int v, const Unit& u) {
    return Value(Measurement::Integer(v, u));
}

Value Value::Boolean(bool b) {
    return Value(Measurement::Boolean(b));
}

Value Value::String(const std::string& s) {
    return Value(Measurement::String(s));
}

Value Value::Complex(std::complex<double> v, const Unit& u) {
    return Value(Measurement::Complex(v, u));
}

Value Value::Vector(const VecXd& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXi& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXcd& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXs& v) {
    return Value(Measurement::Vector(v));
}

Value Value::Matrix(const MatXd& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXi& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXcd& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXs& m) {
    return Value(Measurement::Matrix(m));
}

Value Value::ArrayReal(const std::vector<double>& v, const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<double>(v, u)));
}

Value Value::ArrayInteger(const std::vector<int>& v, const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<int>(v, u)));
}

Value Value::ArrayComplex(const std::vector<std::complex<double>>& v,
                          const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<std::complex<double>>(v, u)));
}

Value Value::ArrayString(const std::vector<std::string>& v) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector(v)));
}

Value Value::ArrayVector(const std::vector<VecXd>& rows, const Unit& u) {
    DataSeries s(DataType::kReal,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXi>& rows, const Unit& u) {
    DataSeries s(DataType::kInteger,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXcd>& rows, const Unit& u) {
    DataSeries s(DataType::kComplex,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXs>& rows) {
    DataSeries s(DataType::kString,
                 {rows.empty() ? Index(0) : rows[0].dimension(0)});
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXd>& rows, const Unit& u) {
    DataSeries s(DataType::kReal,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXi>& rows, const Unit& u) {
    DataSeries s(DataType::kInteger,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXcd>& rows, const Unit& u) {
    DataSeries s(DataType::kComplex,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXs>& rows) {
    DataSeries s(DataType::kString,
                 {rows.empty() ? Index(0) : rows[0].dimension(0),
                  rows.empty() ? Index(0) : rows[0].dimension(1)});
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

}  // namespace rel
