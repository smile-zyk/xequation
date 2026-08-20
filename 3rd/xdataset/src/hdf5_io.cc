#include "dataset_io.h"
#include "hdf5_io.h"

#include <hdf5.h>

#include "block.h"
#include "data_array.h"
#include "data_series.h"
#include "dataset.h"
#include "dimension_spec.h"
#include "touchstone_io.h"

#include <complex>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace xdataset
{

namespace
{

// HDF5 error helper: hide default HDF5 error output, use exceptions instead.
void disable_hdf5_errors()
{
    H5Eset_auto2(H5E_DEFAULT, NULL, NULL);
}

void check_h5(herr_t status, const char* msg)
{
    if (status < 0)
        throw std::runtime_error(std::string("HDF5 error: ") + msg);
}

// -----------------------------------------------------------------------
// Type mapping: xdataset DataType -- HDF5 type id.
// Caller must close the returned type with H5Tclose unless it's a native type.
// -----------------------------------------------------------------------
hid_t h5_type_from(DataType dtype)
{
    switch (dtype)
    {
    case DataType::kReal:    return H5T_NATIVE_DOUBLE;
    case DataType::kInteger: return H5T_NATIVE_INT;
    case DataType::kString:  return H5T_C_S1;  // base, will be sized later
    case DataType::kBoolean: throw std::invalid_argument("Boolean not supported in HDF5");
    case DataType::kComplex:
    {
        hid_t tid = H5Tcreate(H5T_COMPOUND, sizeof(std::complex<double>));
        H5Tinsert(tid, "r", 0,                  H5T_NATIVE_DOUBLE);
        H5Tinsert(tid, "i", sizeof(double),     H5T_NATIVE_DOUBLE);
        return tid;
    }
    }
    throw std::invalid_argument("unknown DataType");
}

// -----------------------------------------------------------------------
// Attribute helpers
// -----------------------------------------------------------------------
void write_str_attr(hid_t loc, const char* name, const std::string& value)
{
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t atype = H5Tcopy(H5T_C_S1);
    H5Tset_size(atype, value.size() + 1);
    H5Tset_strpad(atype, H5T_STR_NULLTERM);
    hid_t attr = H5Acreate2(loc, name, atype, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, atype, value.c_str());
    H5Aclose(attr);
    H5Tclose(atype);
    H5Sclose(space);
}

void write_int_attr(hid_t loc, const char* name, int value)
{
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t attr = H5Acreate2(loc, name, H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, H5T_NATIVE_INT, &value);
    H5Aclose(attr);
    H5Sclose(space);
}

void write_size_attr(hid_t loc, const char* name, std::size_t value)
{
    hid_t space = H5Screate(H5S_SCALAR);
    hid_t attr = H5Acreate2(loc, name, H5T_NATIVE_HSIZE, space, H5P_DEFAULT, H5P_DEFAULT);
    hsize_t v = static_cast<hsize_t>(value);
    H5Awrite(attr, H5T_NATIVE_HSIZE, &v);
    H5Aclose(attr);
    H5Sclose(space);
}

void write_sizes_attr(hid_t loc, const char* name, const std::vector<std::size_t>& sizes)
{
    if (sizes.empty()) return;
    hsize_t n = static_cast<hsize_t>(sizes.size());
    hid_t space = H5Screate_simple(1, &n, NULL);
    std::vector<hsize_t> h5_sizes(n);
    for (hsize_t i = 0; i < n; ++i) h5_sizes[i] = static_cast<hsize_t>(sizes[i]);
    hid_t attr = H5Acreate2(loc, name, H5T_NATIVE_HSIZE, space, H5P_DEFAULT, H5P_DEFAULT);
    H5Awrite(attr, H5T_NATIVE_HSIZE, &h5_sizes[0]);
    H5Aclose(attr);
    H5Sclose(space);
}

void write_dimension_attr(hid_t loc, const DimensionSpec& dim)
{
    write_str_attr(loc, "dim_type", dim.is_regular() ? "regular" : "ragged");
    if (dim.is_regular())
    {
        write_size_attr(loc, "dim_size", dim.regular_size());
    }
    else
    {
        write_sizes_attr(loc, "dim_sizes", dim.ragged_sizes());
    }
}

// -----------------------------------------------------------------------
// Write a DataSeries to an HDF5 Dataset under `group`.
// -----------------------------------------------------------------------
void write_data_series(hid_t group, const std::string& name, const DataSeries& series)
{
    DataType dtype = series.data_type();
    DataKind  kind  = series.data_kind();
    Index     cols  = series.element_count();

    // Build HDF5 dataspace shape: [rows] or [rows, cols] or [rows, R, C]
    std::vector<hsize_t> h5_dims;
    h5_dims.push_back(static_cast<hsize_t>(series.size()));
    if (kind == DataKind::kVector)
        h5_dims.push_back(static_cast<hsize_t>(series.data_shape()[0]));
    else if (kind == DataKind::kMatrix)
    {
        h5_dims.push_back(static_cast<hsize_t>(series.data_shape()[0]));
        h5_dims.push_back(static_cast<hsize_t>(series.data_shape()[1]));
    }

    hid_t fspace = H5Screate_simple(static_cast<int>(h5_dims.size()),
                                     h5_dims.data(), NULL);
    hid_t dtype_id = h5_type_from(dtype);
    bool  owns_dtype = (dtype == DataType::kComplex);

    hid_t dset = H5Dcreate2(group, name.c_str(), dtype_id, fspace,
                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    // Write data
    if (dtype == DataType::kString)
    {
        // Variable-length strings: allocate hvl_t array
        std::vector<char*> buf(series.size());
        std::vector<hvl_t> hvl(series.size());
        for (Index i = 0; i < static_cast<Index>(series.size()); ++i)
        {
            // For string scalar, just one string per row
            const auto& str = series.scalar_at<std::string>(i);
            buf[i] = new char[str.size() + 1];
            std::strcpy(buf[i], str.c_str());
            hvl[i].p   = buf[i];
            hvl[i].len = str.size() + 1;
        }

        // Create variable-length string type
        hid_t strtype = H5Tcopy(H5T_C_S1);
        H5Tset_size(strtype, H5T_VARIABLE);
        check_h5(H5Dwrite(dset, strtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &hvl[0]),
                 "write string dataset");
        H5Tclose(strtype);

        for (Index i = 0; i < static_cast<Index>(series.size()); ++i)
            delete[] buf[i];
    }
    else
    {
        // Numeric: direct write from contiguous data
        const void* ptr = nullptr;
        switch (dtype)
        {
        case DataType::kReal:
            ptr = series.contiguous_data<double>(); break;
        case DataType::kInteger:
            ptr = series.contiguous_data<int>(); break;
        case DataType::kComplex:
            ptr = series.contiguous_data<std::complex<double>>(); break;
        default: break;
        }
        if (ptr)
            check_h5(H5Dwrite(dset, dtype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, ptr),
                     "write numeric dataset");
    }

    // Attributes
    const char* dtype_str = nullptr;
    switch (dtype)
    {
    case DataType::kReal:    dtype_str = "real";    break;
    case DataType::kInteger: dtype_str = "int";     break;
    case DataType::kComplex: dtype_str = "complex"; break;
    case DataType::kString:  dtype_str = "string";  break;
    case DataType::kBoolean: dtype_str = "bool";    break;
    }
    write_str_attr(dset, "dtype", dtype_str);

    const char* kind_str = nullptr;
    switch (kind)
    {
    case DataKind::kScalar: kind_str = "scalar"; break;
    case DataKind::kVector: kind_str = "vector"; break;
    case DataKind::kMatrix: kind_str = "matrix"; break;
    }
    write_str_attr(dset, "kind", kind_str);

    std::string unit_str = series.unit().to_string();
    if (!unit_str.empty())
        write_str_attr(dset, "unit", unit_str);

    H5Dclose(dset);
    if (owns_dtype) H5Tclose(dtype_id);
    H5Sclose(fspace);
}

// -----------------------------------------------------------------------
// Write a Block (as nested HDF5 Groups matching the path, with datasets)
// -----------------------------------------------------------------------
void write_block(hid_t root_group, const std::string& block_path, const Block& block)
{
    // Create nested groups matching the path segments.
    hid_t current = root_group;
    auto parts = Dataset::SplitPath(block_path);
    for (const auto& seg : parts)
    {
        hid_t next = H5Gopen2(current, seg.c_str(), H5P_DEFAULT);
        if (next < 0)
            next = H5Gcreate2(current, seg.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if (current != root_group)
            H5Gclose(current);
        current = next;
    }
    // current is the leaf HDF5 group = the Block's home.

    // Independent DataSeries -- Datasets with dimension attributes
    for (const auto& name : block.independents())
    {
        const IndependentSpec& spec = block.independent_spec(name);
        write_data_series(current, name, spec.data);
        hid_t dset = H5Dopen2(current, name.c_str(), H5P_DEFAULT);
        write_dimension_attr(dset, spec.dimension);
        H5Dclose(dset);
    }

    // Dependent DataSeries -- Datasets
    for (const auto& name : block.dependents())
    {
        const DependentSpec& spec = block.dependent_spec(name);
        write_data_series(current, name, spec.data);
    }

    H5Gclose(current);
}

} // anonymous namespace

// =========================================================================
// Hdf5Writer::Impl
// =========================================================================

class Hdf5Writer::Impl
{
public:
    explicit Impl(const std::string& path)
    {
        disable_hdf5_errors();
        file_ = H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if (file_ < 0)
            throw std::runtime_error("cannot create HDF5 file: " + path);
    }

    ~Impl()
    {
        if (file_ >= 0) H5Fclose(file_);
    }

    void write(const Dataset& dataset)
    {
        // Create root group with Dataset name
        hid_t root = H5Gcreate2(file_, dataset.name().c_str(),
                                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

        // Write each Block
        std::vector<std::string> paths = dataset.GetAllBlockPaths();
        for (const auto& p : paths)
        {
            const Block& block = dataset.GetBlock(p);
            write_block(root, p, block);
        }

        H5Gclose(root);
    }

private:
    hid_t file_;
};

Hdf5Writer::Hdf5Writer(const std::string& file_path)
    : impl_(new Impl(file_path))
{}

Hdf5Writer::~Hdf5Writer() = default;

void Hdf5Writer::Write(const Dataset& dataset)
{
    impl_->write(dataset);
}

// =========================================================================
// Hdf5Reader helpers
// =========================================================================

namespace
{

void load_groups(Dataset& ds, hid_t group, const std::string& prefix);

std::string read_str_attr(hid_t loc, const char* name)
{
    if (!H5Aexists(loc, name)) return "";

    hid_t attr = H5Aopen(loc, name, H5P_DEFAULT);
    hid_t atype = H5Aget_type(attr);

    // Get size
    hid_t space = H5Aget_space(attr);
    hsize_t dims[1];
    int ndims = H5Sget_simple_extent_ndims(space);
    H5Sclose(space);

    std::vector<char> buf(H5Tget_size(atype));
    H5Aread(attr, atype, &buf[0]);

    H5Tclose(atype);
    H5Aclose(attr);

    return std::string(&buf[0]);
}

int read_int_attr(hid_t loc, const char* name, int default_val = 0)
{
    if (!H5Aexists(loc, name)) return default_val;
    hid_t attr = H5Aopen(loc, name, H5P_DEFAULT);
    int val = 0;
    H5Aread(attr, H5T_NATIVE_INT, &val);
    H5Aclose(attr);
    return val;
}

std::size_t read_size_attr(hid_t loc, const char* name, std::size_t default_val = 0)
{
    if (!H5Aexists(loc, name)) return default_val;
    hid_t attr = H5Aopen(loc, name, H5P_DEFAULT);
    hsize_t val = 0;
    H5Aread(attr, H5T_NATIVE_HSIZE, &val);
    H5Aclose(attr);
    return static_cast<std::size_t>(val);
}

std::vector<std::size_t> read_sizes_attr(hid_t loc, const char* name)
{
    if (!H5Aexists(loc, name)) return {};

    hid_t attr = H5Aopen(loc, name, H5P_DEFAULT);
    hid_t space = H5Aget_space(attr);
    hsize_t n = H5Sget_simple_extent_npoints(space);
    H5Sclose(space);

    std::vector<hsize_t> h5_buf(static_cast<std::size_t>(n));
    H5Aread(attr, H5T_NATIVE_HSIZE, &h5_buf[0]);
    H5Aclose(attr);

    std::vector<std::size_t> out;
    for (auto v : h5_buf) out.push_back(static_cast<std::size_t>(v));
    return out;
}

DimensionSpec read_dimension_attr(hid_t loc)
{
    std::string dim_type = read_str_attr(loc, "dim_type");
    if (dim_type == "regular")
    {
        return DimensionSpec::Regular(read_size_attr(loc, "dim_size"));
    }
    else if (dim_type == "ragged")
    {
        return DimensionSpec::Ragged(read_sizes_attr(loc, "dim_sizes"));
    }
    throw std::runtime_error("unknown dim_type: " + dim_type);
}

// -----------------------------------------------------------------------
// Read a DataSeries from an HDF5 Dataset.
// -----------------------------------------------------------------------
DataSeries read_data_series(hid_t loc, const std::string& name)
{
    hid_t dset = H5Dopen2(loc, name.c_str(), H5P_DEFAULT);

    // Read attributes
    std::string dtype_str = read_str_attr(dset, "dtype");
    std::string kind_str  = read_str_attr(dset, "kind");
    std::string unit_str  = read_str_attr(dset, "unit");

    // Get dataspace
    hid_t fspace = H5Dget_space(dset);
    int ndims = H5Sget_simple_extent_ndims(fspace);
    std::vector<hsize_t> h5_dims(static_cast<std::size_t>(ndims));
    H5Sget_simple_extent_dims(fspace, h5_dims.data(), NULL);
    H5Sclose(fspace);

    // Determine rows and inner shape
    std::size_t rows = static_cast<std::size_t>(h5_dims[0]);
    Index cols = 0;
    Index cell_rows = 0, cell_cols = 0;

    if (kind_str == "vector" && ndims >= 2)
        cols = static_cast<Index>(h5_dims[1]);
    else if (kind_str == "matrix" && ndims >= 3)
    {
        cell_rows = static_cast<Index>(h5_dims[1]);
        cell_cols = static_cast<Index>(h5_dims[2]);
    }

    // Total flat elements
    std::size_t total = rows * static_cast<std::size_t>(
        cols > 0 ? cols : (cell_rows * cell_cols > 0 ? cell_rows * cell_cols : 1));

    DataSeries out;
    Unit unit;
    if (!unit_str.empty()) unit = Unit::parse(unit_str);

    hid_t memtype = H5Dget_type(dset);

    if (dtype_str == "real")
    {
        std::vector<double> buf(total);
        H5Dread(dset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &buf[0]);

        if (kind_str == "scalar")
            out = DataSeries::CreateScalarFromMemory<double>(&buf[0], rows, unit);
        else if (kind_str == "vector")
            out = DataSeries::CreateVectorFromMemory<double>(cols, &buf[0], rows * cols, unit);
        else if (kind_str == "matrix")
            out = DataSeries::CreateMatrixFromMemory<double>(cell_rows, cell_cols, &buf[0], total, unit);
    }
    else if (dtype_str == "int")
    {
        std::vector<int> buf(total);
        H5Dread(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &buf[0]);

        if (kind_str == "scalar")
            out = DataSeries::CreateScalarFromMemory<int>(&buf[0], rows, unit);
        else if (kind_str == "vector")
            out = DataSeries::CreateVectorFromMemory<int>(cols, &buf[0], rows * cols, unit);
        else if (kind_str == "matrix")
            out = DataSeries::CreateMatrixFromMemory<int>(cell_rows, cell_cols, &buf[0], total, unit);
    }
    else if (dtype_str == "complex")
    {
        std::vector<std::complex<double>> buf(total);
        H5Dread(dset, memtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &buf[0]);

        if (kind_str == "scalar")
            out = DataSeries::CreateScalarFromMemory<std::complex<double>>(&buf[0], rows, unit);
        else if (kind_str == "vector")
            out = DataSeries::CreateVectorFromMemory<std::complex<double>>(cols, &buf[0], rows * cols, unit);
        else if (kind_str == "matrix")
            out = DataSeries::CreateMatrixFromMemory<std::complex<double>>(cell_rows, cell_cols, &buf[0], total, unit);
    }
    else
    {
        throw std::runtime_error("unsupported dtype: " + dtype_str);
    }

    H5Tclose(memtype);
    H5Dclose(dset);
    return out;
}

// -----------------------------------------------------------------------
// Read a Block from an HDF5 Group.
// -----------------------------------------------------------------------
Block read_block(hid_t group, const std::string& block_name)
{
    hid_t bg = H5Gopen2(group, block_name.c_str(), H5P_DEFAULT);

    BlockCreateInfo info;

    // Iterate child datasets
    hsize_t num_objs = 0;
    H5Gget_num_objs(bg, &num_objs);

    // First pass: collect dataset names and see which have dim_type attr
    std::vector<std::string> indep_names, dep_names;
    for (hsize_t i = 0; i < num_objs; ++i)
    {
        char obj_name[256];
        H5Gget_objname_by_idx(bg, i, obj_name, sizeof(obj_name));
        std::string oname(obj_name);

        int otype = H5Gget_objtype_by_idx(bg, i);
        if (otype != H5G_DATASET) continue;

        // Check if this dataset has a dim_type attribute -- independent
        hid_t dset = H5Dopen2(bg, oname.c_str(), H5P_DEFAULT);
        bool has_dim = H5Aexists(dset, "dim_type");
        H5Dclose(dset);

        if (has_dim)
            indep_names.push_back(oname);
        else
            dep_names.push_back(oname);
    }

    // Read independents
    for (const auto& name : indep_names)
    {
        DataSeries data = read_data_series(bg, name);
        hid_t dset = H5Dopen2(bg, name.c_str(), H5P_DEFAULT);
        DimensionSpec dim = read_dimension_attr(dset);
        H5Dclose(dset);

        IndependentSpec is = {name, std::move(data), dim};
        info.independent_specs.push_back(is);
    }

    // Read dependents
    for (const auto& name : dep_names)
    {
        DependentSpec ds = {name, read_data_series(bg, name)};
        info.dependent_specs.push_back(ds);
    }

    H5Gclose(bg);

    return Block(info);
}

/// Walk an HDF5 group and recursively load Blocks into the Dataset.
void load_groups(Dataset& ds, hid_t group, const std::string& prefix)
{
    hsize_t num_objs = 0;
    H5Gget_num_objs(group, &num_objs);

    for (hsize_t i = 0; i < num_objs; ++i)
    {
        char obj_name[256];
        H5Gget_objname_by_idx(group, i, obj_name, sizeof(obj_name));
        std::string oname(obj_name);

        int otype = H5Gget_objtype_by_idx(group, i);
        if (otype == H5G_GROUP)
        {
            hid_t child = H5Gopen2(group, oname.c_str(), H5P_DEFAULT);
            hsize_t child_objs = 0;
            H5Gget_num_objs(child, &child_objs);

            // Check if this group contains datasets (is a Block) or only subgroups.
            bool has_datasets = false;
            for (hsize_t j = 0; j < child_objs; ++j)
            {
                if (H5Gget_objtype_by_idx(child, j) == H5G_DATASET)
                {
                    has_datasets = true;
                    break;
                }
            }

            std::string path = prefix.empty() ? oname : prefix + "/" + oname;

            if (has_datasets)
            {
                // This group is a Block.
                ds.AddBlock(path, read_block(group, oname));
            }

            // Recurse into subgroups.
            load_groups(ds, child, path);

            H5Gclose(child);
        }
    }
}

} // anonymous namespace

// =========================================================================
// Hdf5Reader::Impl
// =========================================================================

class Hdf5Reader::Impl
{
public:
    explicit Impl(const std::string& path)
    {
        disable_hdf5_errors();
        file_ = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
        if (file_ < 0)
            throw std::runtime_error("cannot open HDF5 file: " + path);
    }

    ~Impl()
    {
        if (file_ >= 0) H5Fclose(file_);
    }

    Dataset read()
    {
        // Get the first (and only) root group = the Dataset name.
        hsize_t num_objs = 0;
        H5Gget_num_objs(file_, &num_objs);

        if (num_objs != 1)
            throw std::runtime_error("HDF5 file must contain exactly one root group");

        char root_name[256];
        H5Gget_objname_by_idx(file_, 0, root_name, sizeof(root_name));

        Dataset ds(root_name);
        hid_t root = H5Gopen2(file_, root_name, H5P_DEFAULT);
        load_groups(ds, root, "");
        H5Gclose(root);
        return ds;
    }

private:
    hid_t file_;
};

Hdf5Reader::Hdf5Reader(const std::string& file_path)
    : impl_(new Impl(file_path))
{}

Hdf5Reader::~Hdf5Reader() = default;

Dataset Hdf5Reader::Read()
{
    return impl_->read();
}

// =========================================================================
// DatasetIO
// =========================================================================

/* static */
std::unique_ptr<IDatasetWriter> DatasetIO::CreateWriter(
    const std::string& format,
    const std::string& path)
{
    if (format == "hdf5")
        return std::unique_ptr<IDatasetWriter>(new Hdf5Writer(path));
    throw std::invalid_argument("unsupported format: " + format);
}

/* static */
std::unique_ptr<IDatasetReader> DatasetIO::CreateReader(
    const std::string& format,
    const std::string& path)
{
    if (format == "hdf5")
        return std::unique_ptr<IDatasetReader>(new Hdf5Reader(path));
    if (format == "touchstone" || format == "snp")
        return std::unique_ptr<IDatasetReader>(new TouchstoneReader(path));
    throw std::invalid_argument("unsupported format: " + format);
}

/* static */
void DatasetIO::Save(const Dataset& dataset,
                     const std::string& format,
                     const std::string& path)
{
    auto writer = CreateWriter(format, path);
    writer->Write(dataset);
}

/* static */
Dataset DatasetIO::Load(const std::string& format,
                        const std::string& path)
{
    auto reader = CreateReader(format, path);
    return reader->Read();
}

} // namespace xdataset
