#ifndef XDATASET_DATASET_IO_H
#define XDATASET_DATASET_IO_H

#include <memory>
#include <string>

#include "xdataset_predefine.h"

namespace xdataset
{

class Dataset;

// =========================================================================
// IDatasetWriter -- abstract output for Dataset persistence
// =========================================================================
//
// Implementations write a Dataset to a file or stream.  The base class
// is format-agnostic; concrete subclasses handle HDF5, JSON, etc.
//
// Usage:
//   auto w = DatasetIO::CreateWriter("hdf5", "noise.xdataset");
//   w->Write(dataset);
// =========================================================================

class XDATASET_API IDatasetWriter
{
public:
    virtual ~IDatasetWriter() = default;

    /// Write the full Dataset (tree, blocks, arrays, metadata).
    virtual void Write(const Dataset& dataset) = 0;
};

// =========================================================================
// IDatasetReader -- abstract input for Dataset loading
// =========================================================================

class XDATASET_API IDatasetReader
{
public:
    virtual ~IDatasetReader() = default;

    /// Read and reconstruct a Dataset.
    virtual Dataset Read() = 0;
};

// =========================================================================
// DatasetIO -- factory + convenience Save / Load
// =========================================================================

class XDATASET_API DatasetIO
{
public:
    /// Create a writer for the given format.
    /// @param format  "hdf5" (extensible)
    /// @param path    output file path
    static std::unique_ptr<IDatasetWriter> CreateWriter(
        const std::string& format,
        const std::string& path);

    /// Create a reader for the given format.
    /// @param format  "hdf5" (extensible)
    /// @param path    input file path
    static std::unique_ptr<IDatasetReader> CreateReader(
        const std::string& format,
        const std::string& path);

    /// Convenience: save a Dataset using the writer for `format`.
    static void Save(const Dataset& dataset,
                     const std::string& format,
                     const std::string& path);

    /// Convenience: load a Dataset using the reader for `format`.
    static Dataset Load(const std::string& format,
                        const std::string& path);
};

} // namespace xdataset

#endif // XDATASET_DATASET_IO_H
