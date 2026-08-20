#ifndef XDATASET_DATA_ARRAY_IO_H
#define XDATASET_DATA_ARRAY_IO_H

#include <memory>
#include <string>

#include "xdataset_predefine.h"

namespace xdataset
{

class DataArray;

// =========================================================================
// IDataArrayWriter -- abstract output for DataArray persistence
// =========================================================================

class XDATASET_API IDataArrayWriter
{
public:
    virtual ~IDataArrayWriter() = default;
    virtual void Write(const DataArray& array) = 0;
};

// =========================================================================
// IDataArrayReader -- abstract input for DataArray loading
// =========================================================================

class XDATASET_API IDataArrayReader
{
public:
    virtual ~IDataArrayReader() = default;
    virtual DataArray Read() = 0;
};

// =========================================================================
// DataArrayIO -- factory + convenience Save / Load
// =========================================================================

class XDATASET_API DataArrayIO
{
public:
    static std::unique_ptr<IDataArrayWriter> CreateWriter(
        const std::string& format,
        const std::string& path);

    static std::unique_ptr<IDataArrayReader> CreateReader(
        const std::string& format,
        const std::string& path);

    static void Save(const DataArray& array,
                     const std::string& format,
                     const std::string& path);

    static DataArray Load(const std::string& format,
                          const std::string& path);
};

} // namespace xdataset

#endif // XDATASET_DATA_ARRAY_IO_H
