#ifndef DATA_FRAME_H
#define DATA_FRAME_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "measurement.h"

namespace xdataset
{
    class DataSeries;
    class Block;
    class DataArray;

    // =========================================================================
    // DataFrameRow -- a single row in a DataFrame table
    // =========================================================================
    struct XDATASET_API DataFrameRow
    {
        std::vector<Index>       multi_index;
        std::vector<Measurement> fields;

        std::string FormatMultiIndex() const;
    };

    // =========================================================================
    // DataFrame -- lazy-loading tabular container
    // =========================================================================
    //
    // Rows are loaded on demand in fixed-size chunks.  The concrete frame
    // types are internal to the library -- they are created through the
    // static factories below (FromBlock / FromDataArray / FromMeasurement)
    // and are also produced by Block::GetOrCreateDataFrame(),
    // DataArray::GetOrCreateDataFrame() and Measurement::to_dataframe().
    // Callers interact with the DataFrame base interface only.
    // =========================================================================

    class XDATASET_API DataFrame
    {
    public:
        /// Virtual so derived objects held via unique_ptr<DataFrame> are
        /// destroyed correctly by the library's caches.
        virtual ~DataFrame() = default;

        // ---- static factories ---------------------------------------------

        /// Build a frame tabulating a Block's independent/dependent variables.
        static std::unique_ptr<DataFrame> FromBlock(const Block& block);

        /// Build a frame tabulating a DataArray.
        /// @param variable      The DataArray to tabulate.
        /// @param variable_name Column header for dependent data (default "UnNamed").
        static std::unique_ptr<DataFrame> FromDataArray(
            const DataArray& variable,
            std::string variable_name = "UnNamed");

        /// Build a single-row frame displaying one Measurement.
        static std::unique_ptr<DataFrame> FromMeasurement(
            const Measurement& measurement,
            std::string name);

        // ---- accessors ----------------------------------------------------

        const std::vector<std::string>& headers()   const { return headers_;   }
        std::size_t                     row_count() const { return total_rows_; }

        virtual const DataFrameRow& GetRow(Index row) const;

        /// Update the dependent column header without rebuilding rows.
        /// No-op for frames without a dependent column (Block, Measurement).
        virtual void UpdateVariableName(std::string variable_name);

        std::string ToCsv() const;
        void        WriteToCsv(const std::string& file_path) const;

        /// Return a human-readable ASCII table representation.
        /// Only the first @p max_display_rows data rows are shown;
        /// when row_count() exceeds this limit, an ellipsis row and
        /// a footer indicate how many rows were omitted.
        /// @param max_display_rows  Maximum number of data rows to show
        ///                         (default 32).
        std::string to_string(std::size_t max_display_rows = 32) const;

    protected:
        using RowGenerator = std::function<std::vector<DataFrameRow>(
            Index start_row,
            Index end_row)>;

        DataFrame() = default;

        void ConfigureDynamic(std::vector<std::string> headers,
                              std::size_t total_rows,
                              RowGenerator generator,
                              std::size_t chunk_size = 128);

        /// Configure with pre-built rows -- no lazy loading, no generator.
        /// Suitable for small DataFrames where caching is unnecessary.
        void ConfigureStatic(std::vector<std::string> headers,
                             std::vector<DataFrameRow> rows);

        /// Replace headers in-place (same count, only labels change).
        /// Subclasses use this for on-the-fly header refresh (e.g. rename).
        void set_headers(std::vector<std::string> headers);

    private:
        void EnsureChunkLoaded(Index chunk_idx) const;

        std::vector<std::string>   headers_;
        std::size_t                total_rows_  = 0;
        std::size_t                chunk_size_  = 128;
        RowGenerator               generator_;

        mutable std::vector<DataFrameRow>  rows_;
        mutable std::vector<bool>           loaded_chunks_;
    };

} // namespace xdataset

#endif  // DATA_FRAME_H
