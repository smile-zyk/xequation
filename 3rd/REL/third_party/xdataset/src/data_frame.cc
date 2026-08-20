#include "data_frame.h"
#include "block.h"
#include "data_series.h"
#include "data_array.h"
#include "measurement.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace xdataset
{
    namespace
    {
        std::string EscapeCsvField(const std::string& field)
        {
            bool need_quote = false;
            for (char ch : field)
            {
                if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r')
                {
                    need_quote = true;
                    break;
                }
            }

            if (!need_quote)
            {
                return field;
            }

            std::string escaped;
            escaped.reserve(field.size() + 2);
            escaped.push_back('"');
            for (char ch : field)
            {
                if (ch == '"')
                {
                    escaped.push_back('"');
                }
                escaped.push_back(ch);
            }
            escaped.push_back('"');
            return escaped;
        }
    } // namespace

    // =========================================================================
    // Concrete frame types (private to this translation unit -- the public
    // DataFrame API exposes only the static factories below).
    // =========================================================================

    class BlockDataFrame : public DataFrame
    {
    public:
        explicit BlockDataFrame(const Block& block);
    };

    class DataArrayDataFrame : public DataFrame
    {
    public:
        explicit DataArrayDataFrame(const DataArray& variable,
                                    std::string variable_name = "UnNamed");

        void UpdateVariableName(std::string variable_name) override;

    private:
        void rebuild_headers();

        const DataArray* data_array_;
        std::string      variable_name_;
    };

    class MeasurementDataFrame : public DataFrame
    {
    public:
        MeasurementDataFrame(const Measurement& measurement, std::string name);

        const DataFrameRow& GetRow(Index row) const override;

    private:
        DataFrameRow row_;
    };

    // =========================================================================
    // DataFrameRow
    // =========================================================================

    std::string DataFrameRow::FormatMultiIndex() const
    {
        std::ostringstream oss;
        for (std::size_t i = 0; i < multi_index.size(); ++i)
        {
            if (i > 0) oss << ",";
            oss << multi_index[i];
        }
        return oss.str();
    }

    // =========================================================================
    // Internal helpers (file-local)
    // =========================================================================

    static std::vector<std::string> ExpandHeadersForSeries(
        const std::string& base_name,
        const DataSeries& series)
    {
        std::vector<std::string> headers;
        if (series.data_kind() == DataKind::kScalar)
        {
            headers.push_back(base_name);
            return headers;
        }

        if (series.data_kind() == DataKind::kVector)
        {
            const Index width = series.data_shape()[0];
            headers.reserve(static_cast<std::size_t>(width));
            for (Index i = 0; i < width; ++i)
            {
                headers.push_back(base_name + "(" + std::to_string(static_cast<std::size_t>(i) + 1) + ")");
            }
            return headers;
        }

        if (series.data_kind() == DataKind::kMatrix)
        {
            const Index rows = series.data_shape()[0];
            const Index cols = series.data_shape()[1];
            headers.reserve(static_cast<std::size_t>(rows * cols));
            for (Index r = 0; r < rows; ++r)
            {
                for (Index c = 0; c < cols; ++c)
                {
                    headers.push_back(
                        base_name +
                        "(" + std::to_string(static_cast<std::size_t>(r) + 1) +
                        "," + std::to_string(static_cast<std::size_t>(c) + 1) +
                        ")");
                }
            }
            return headers;
        }

        throw std::logic_error("unsupported data kind");
    }

    static std::vector<Measurement> ExpandToColumns(const DataSeries& series, Index row)
    {
        Measurement m = series.measurement_at(row);
        std::vector<Measurement> result;

        if (series.data_kind() == DataKind::kScalar)
            return {m};

        if (series.data_kind() == DataKind::kVector)
        {
            const Index width = m.shape()[0];
            result.reserve(static_cast<std::size_t>(width));
            for (Index i = 0; i < width; ++i)
                result.push_back(m.element_at(i));
            return result;
        }

        // Matrix
        const Index rows = m.shape()[0];
        const Index cols = m.shape()[1];
        result.reserve(static_cast<std::size_t>(rows * cols));
        for (Index r = 0; r < rows; ++r)
            for (Index c = 0; c < cols; ++c)
                result.push_back(m.element_at(r, c));
        return result;
    }

    static std::string ExpandToString(const DataSeries& series, Index row)
    {
        const std::vector<Measurement> cols = ExpandToColumns(series, row);
        if (cols.empty())
        {
            return {};
        }
        if (cols.size() == 1)
        {
            return cols[0].to_string();
        }

        std::ostringstream oss;
        oss << "[";

        if (series.data_kind() == DataKind::kMatrix)
        {
            const Index mat_rows = series.data_shape()[0];
            const Index mat_cols = series.data_shape()[1];
            for (Index r = 0; r < mat_rows; ++r)
            {
                if (r > 0) oss << ",";
                oss << "[";
                for (Index c = 0; c < mat_cols; ++c)
                {
                    if (c > 0) oss << ",";
                    oss << cols[static_cast<std::size_t>(r * mat_cols + c)].to_string();
                }
                oss << "]";
            }
        }
        else
        {
            for (std::size_t i = 0; i < cols.size(); ++i)
            {
                if (i > 0) oss << ",";
                oss << cols[i].to_string();
            }
        }

        oss << "]";
        return oss.str();
    }

    // =========================================================================
    // DataFrame: lazy loading
    // =========================================================================

    void DataFrame::ConfigureDynamic(std::vector<std::string> headers,
                              std::size_t total_rows,
                              RowGenerator generator,
                              std::size_t chunk_size)
    {
        headers_     = std::move(headers);
        total_rows_  = total_rows;
        chunk_size_  = (chunk_size > 0) ? chunk_size : 256;
        generator_   = std::move(generator);

        // Pre-allocate cache slots so operator[] is safe for any valid row.
        rows_.resize(total_rows_);
        loaded_chunks_.assign((total_rows_ + chunk_size_ - 1) / chunk_size_, false);
    }

    void DataFrame::ConfigureStatic(std::vector<std::string> headers,
                                    std::vector<DataFrameRow> rows)
    {
        headers_    = std::move(headers);
        total_rows_ = rows.size();
        rows_       = std::move(rows);
        // No generator, no chunk loading -- rows are fully materialised.
        // Mark all rows as already loaded so base GetRow / EnsureChunkLoaded
        // works without a generator.
        loaded_chunks_.assign(
            total_rows_ > 0 ? 1 : 0, true);
        chunk_size_ = static_cast<std::size_t>(total_rows_);
    }

    const DataFrameRow& DataFrame::GetRow(Index row) const
    {
        EnsureChunkLoaded(row / static_cast<Index>(chunk_size_));
        return rows_[static_cast<std::size_t>(row)];
    }

    void DataFrame::EnsureChunkLoaded(Index chunk_idx) const
    {
        const std::size_t ci = static_cast<std::size_t>(chunk_idx);
        if (loaded_chunks_[ci])
        {
            return;
        }

        const Index start = static_cast<Index>(ci * chunk_size_);
        const Index end   = static_cast<Index>((std::min)((ci + 1) * chunk_size_, total_rows_));

        std::vector<DataFrameRow> chunk_rows = generator_(start, end);

        for (std::size_t i = 0; i < chunk_rows.size(); ++i)
        {
            rows_[static_cast<std::size_t>(start) + i] = std::move(chunk_rows[i]);
        }

        loaded_chunks_[ci] = true;
    }

    // =========================================================================
    // WriteToCsv / ToCsv
    // =========================================================================

    std::string DataFrame::ToCsv() const
    {
        std::string csv;

        // Header row: leading comma for multi-index column
        for (const auto& header : headers_)
        {
            csv.push_back(',');
            csv += EscapeCsvField(header);
        }
        csv.push_back('\n');

        // Data rows (lazy -- triggers chunk loads on demand)
        for (std::size_t i = 0; i < total_rows_; ++i)
        {
            const DataFrameRow& row = GetRow(static_cast<Index>(i));
            csv += EscapeCsvField(row.FormatMultiIndex());

            for (const Measurement& field : row.fields)
            {
                csv.push_back(',');
                csv += EscapeCsvField(field.to_string());
            }
            csv.push_back('\n');
        }

        return csv;
    }

    void DataFrame::WriteToCsv(const std::string& file_path) const
    {
        if (file_path.empty())
        {
            throw std::invalid_argument("csv file path must not be empty");
        }

        std::ofstream ofs(file_path.c_str(), std::ios::out | std::ios::trunc);
        if (!ofs.is_open())
        {
            throw std::runtime_error("failed to open csv file: " + file_path);
        }

        ofs << ToCsv();

        if (!ofs.good())
        {
            throw std::runtime_error("failed to write csv file: " + file_path);
        }
    }

    void DataFrame::set_headers(std::vector<std::string> headers)
    {
        headers_ = std::move(headers);
    }

    // =========================================================================
    // to_string -- human-readable ASCII table
    // =========================================================================

    std::string DataFrame::to_string(std::size_t max_display_rows) const
    {
        // Column list: "#" for multi-index, then headers_
        std::vector<std::string> columns;
        columns.reserve(1 + headers_.size());
        columns.emplace_back("#");
        columns.insert(columns.end(), headers_.begin(), headers_.end());

        const std::size_t num_cols = columns.size();
        const std::size_t total    = total_rows_;

        // ---- decide which rows to display (head only) ----
        const bool        truncated = total > max_display_rows;
        const std::size_t shown     = truncated ? max_display_rows : total;
        const std::size_t omitted   = total - shown;

        std::vector<std::size_t> display_indices;
        display_indices.reserve(shown);
        for (std::size_t i = 0; i < shown; ++i)
            display_indices.push_back(i);

        // ---- compute column widths ----
        std::vector<std::size_t> widths(num_cols, 0);
        for (std::size_t c = 0; c < num_cols; ++c)
            widths[c] = columns[c].size();

        for (std::size_t row_idx : display_indices)
        {
            const DataFrameRow& row = GetRow(static_cast<Index>(row_idx));
            const std::string mi = row.FormatMultiIndex();
            if (mi.size() > widths[0])
                widths[0] = mi.size();
            for (std::size_t c = 0; c < row.fields.size(); ++c)
            {
                const std::string fs = row.fields[c].to_string();
                const std::size_t col = c + 1;
                if (col < widths.size() && fs.size() > widths[col])
                    widths[col] = fs.size();
            }
        }

        // Ensure ellipsis "..." fits in every column when truncated
        if (truncated)
        {
            for (std::size_t c = 0; c < num_cols; ++c)
                if (widths[c] < 3)
                    widths[c] = 3;
        }

        // ---- helper lambdas ----
        static const std::string kEmpty;

        auto make_border = [&](char corner, char tee, char end, char fill)
        {
            std::string s;
            s += corner;
            for (std::size_t c = 0; c < num_cols; ++c)
            {
                if (c > 0) s += tee;
                s.append(widths[c] + 2, fill);
            }
            s += end;
            s += '\n';
            return s;
        };

        auto make_row = [&](const std::vector<std::string>& values)
        {
            std::string s;
            s += '|';
            for (std::size_t c = 0; c < num_cols; ++c)
            {
                const std::string& val = c < values.size() ? values[c] : kEmpty;
                s += ' ';
                s += val;
                s.append(widths[c] - val.size() + 1, ' ');
                s += '|';
            }
            s += '\n';
            return s;
        };

        // ---- build output ----
        std::ostringstream oss;

        oss << make_border('+', '+', '+', '-');
        oss << make_row(columns);
        oss << make_border('+', '+', '+', '-');

        for (std::size_t row_idx : display_indices)
        {
            const DataFrameRow& row = GetRow(static_cast<Index>(row_idx));
            std::vector<std::string> vals;
            vals.reserve(num_cols);
            vals.push_back(row.FormatMultiIndex());
            for (const Measurement& field : row.fields)
                vals.push_back(field.to_string());
            oss << make_row(vals);
        }

        if (truncated)
        {
            std::vector<std::string> dots(num_cols, "...");
            oss << make_row(dots);
        }

        oss << make_border('+', '+', '+', '-');

        if (truncated)
            oss << "(" << omitted << " row" << (omitted != 1 ? "s" : "")
                << " omitted, " << shown << " shown)\n";

        return oss.str();
    }

    // =========================================================================
    // BlockDataFrame
    // =========================================================================

    BlockDataFrame::BlockDataFrame(const Block& block)
    {
        std::vector<std::string> all_headers;
        std::vector<DimensionSpec> dims;
        std::vector<std::string> indep_names = block.independents();
        dims.reserve(indep_names.size());

        for (const std::string& indep_name : indep_names)
        {
            const IndependentSpec& iv = block.independent_spec(indep_name);
            const std::vector<std::string> hdrs = ExpandHeadersForSeries(indep_name, iv.data);
            all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
            dims.push_back(iv.dimension);
        }

        std::vector<std::string> dep_names = block.dependents();
        for (const std::string& dep_name : dep_names)
        {
            const DependentSpec& dv = block.dependent_spec(dep_name);
            const std::vector<std::string> hdrs = ExpandHeadersForSeries(dep_name, dv.data);
            all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
        }

        MultiDimensionSpec traversal_spec;
        for (const auto& d : dims)
            traversal_spec.add_dimension(d);
        const std::size_t total_rows = dims.empty() ? 0 : traversal_spec.compute_cell_count();
        const std::size_t total_headers = all_headers.size();
        const Block*      b = &block;

        ConfigureDynamic(std::move(all_headers), total_rows,
            [b, traversal_spec, total_headers, indep_names, dep_names](
                Index start, Index end) -> std::vector<DataFrameRow>
            {
                std::vector<DataFrameRow> result;
                result.reserve(static_cast<std::size_t>(end - start));
                traversal_spec.for_each_leaf_row(
                    [&](const MultiDimensionSpec::LeafRow& leaf_row)
                    {
                        DataFrameRow row;
                        row.multi_index = leaf_row.multi_index;
                        row.fields.reserve(total_headers);

                        const auto& dim_ri = leaf_row.dimension_row_indices;
                        for (std::size_t d = 0; d < indep_names.size(); ++d)
                        {
                            const IndependentSpec& iv = b->independent_spec(indep_names[d]);
                            const std::vector<Measurement> vals = ExpandToColumns(iv.data, dim_ri[d]);
                            row.fields.insert(row.fields.end(), vals.begin(), vals.end());
                        }
                        for (const std::string& dn : dep_names)
                        {
                            const DependentSpec& dv = b->dependent_spec(dn);
                            const std::vector<Measurement> vals = ExpandToColumns(dv.data, leaf_row.row_flat);
                            row.fields.insert(row.fields.end(), vals.begin(), vals.end());
                        }
                        result.push_back(std::move(row));
                    },
                    start, end);
                return result;
            });
    }

    // =========================================================================
    // DataArrayDataFrame
    // =========================================================================

    DataArrayDataFrame::DataArrayDataFrame(const DataArray& dataArray,
                                           std::string variable_name)
        : data_array_(&dataArray),
          variable_name_(std::move(variable_name))
    {
        const MultiDimensionSpec& spec = dataArray.multi_dimension_spec();
        if (spec.empty())
            throw std::logic_error("DataArray table view requires non-empty dimensions");

        const std::size_t rank = spec.rank();

        // Collect all columns: iterate datas() and classify each entry.
        std::vector<std::pair<std::string, const DataSeries*>> indep_columns;
        const DataSeries* dep_series = nullptr;
        std::vector<std::string> all_headers;

        std::size_t pos = 0;
        for (const auto& item : dataArray.datas())
        {
            const bool is_self = (item.first == DataArray::kSelf);
            std::string col_name;

            if (is_self)
            {
                col_name = variable_name_;
                if (dataArray.data_kind() == DataArrayKind::kDependent)
                {
                    // kSelf is the dependent data column.
                    dep_series = &item.second;
                }
                else
                {
                    // Independent: kSelf is also an indep dimension column.
                    indep_columns.push_back(std::make_pair(col_name, &item.second));
                }
            }
            else
            {
                col_name = item.first;
                indep_columns.push_back(std::make_pair(col_name, &item.second));
            }

            if (!is_self || dataArray.data_kind() != DataArrayKind::kDependent)
            {
                const std::vector<std::string> hdrs = ExpandHeadersForSeries(col_name, item.second);
                all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
            }
            ++pos;
        }

        // Add dependent column headers last (after indep columns).
        if (dep_series != nullptr)
        {
            const std::vector<std::string> hdrs = ExpandHeadersForSeries(variable_name_, *dep_series);
            all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
        }

        if (indep_columns.size() != rank)
            throw std::logic_error("independent columns count must match MultiDimensionSpec rank");

        const std::size_t total_rows   = spec.compute_cell_count();
        const std::size_t total_headers = all_headers.size();

        const DataArray* da = data_array_;
        ConfigureDynamic(std::move(all_headers), total_rows,
            [da, indep_columns, total_headers, dep_series](
                Index start, Index end) -> std::vector<DataFrameRow>
            {
                std::vector<DataFrameRow> result;
                result.reserve(static_cast<std::size_t>(end - start));
                da->for_each_leaf_row(
                    [&](const MultiDimensionSpec::LeafRow& leaf_row)
                    {
                        DataFrameRow row;
                        row.multi_index = leaf_row.multi_index;
                        row.fields.reserve(total_headers);

                        const auto& dim_ri = leaf_row.dimension_row_indices;
                        for (std::size_t d = 0; d < indep_columns.size(); ++d)
                        {
                            const DataSeries* is = indep_columns[d].second;
                            if (dim_ri[d] < 0 || static_cast<std::size_t>(dim_ri[d]) >= is->size())
                                throw std::out_of_range("expanded independent row index out of bounds");
                            const std::vector<Measurement> vals = ExpandToColumns(*is, dim_ri[d]);
                            row.fields.insert(row.fields.end(), vals.begin(), vals.end());
                        }
                        if (dep_series != nullptr)
                        {
                            if (leaf_row.row_flat < 0 || static_cast<std::size_t>(leaf_row.row_flat) >= dep_series->size())
                                throw std::out_of_range("dependent data row index out of bounds");
                            const std::vector<Measurement> vals = ExpandToColumns(*dep_series, leaf_row.row_flat);
                            row.fields.insert(row.fields.end(), vals.begin(), vals.end());
                        }
                        result.push_back(std::move(row));
                    },
                    start, end);
                return result;
            });
    }

    void DataArrayDataFrame::UpdateVariableName(std::string variable_name)
    {
        if (variable_name_ == variable_name)
            return;
        variable_name_ = std::move(variable_name);
        rebuild_headers();
    }

    void DataArrayDataFrame::rebuild_headers()
    {
        std::vector<std::string> all_headers;

        for (const auto& item : data_array_->datas())
        {
            const bool is_self = (item.first == DataArray::kSelf);
            std::string col_name = is_self ? variable_name_ : item.first;

            if (is_self && data_array_->data_kind() == DataArrayKind::kDependent)
            {
                // Dependent kSelf column: add after indep columns.
                continue;
            }

            const std::vector<std::string> hdrs = ExpandHeadersForSeries(col_name, item.second);
            all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
        }

        if (data_array_->data_kind() == DataArrayKind::kDependent)
        {
            const std::vector<std::string> hdrs = ExpandHeadersForSeries(variable_name_, data_array_->data());
            all_headers.insert(all_headers.end(), hdrs.begin(), hdrs.end());
        }

        set_headers(std::move(all_headers));
    }

    // =========================================================================
    // MeasurementDataFrame
    // =========================================================================

    MeasurementDataFrame::MeasurementDataFrame(const Measurement& measurement,
                                               std::string name)
    {
        const DataKind kind  = measurement.data_kind();
        const DataShape& shape = measurement.shape();

        // Build headers according to kind / shape, using the caller-supplied name.
        std::vector<std::string> all_headers;
        if (kind == DataKind::kScalar)
        {
            all_headers.push_back(name);
        }
        else if (kind == DataKind::kVector)
        {
            const Index width = shape[0];
            all_headers.reserve(static_cast<std::size_t>(width));
            for (Index i = 0; i < width; ++i)
            {
                all_headers.push_back(
                    name + "(" + std::to_string(static_cast<std::size_t>(i) + 1) + ")");
            }
        }
        else if (kind == DataKind::kMatrix)
        {
            const Index rows = shape[0];
            const Index cols = shape[1];
            all_headers.reserve(static_cast<std::size_t>(rows * cols));
            for (Index r = 0; r < rows; ++r)
            {
                for (Index c = 0; c < cols; ++c)
                {
                    all_headers.push_back(
                        name +
                        "(" + std::to_string(static_cast<std::size_t>(r) + 1) +
                        "," + std::to_string(static_cast<std::size_t>(c) + 1) +
                        ")");
                }
            }
        }
        else
        {
            throw std::logic_error("unsupported data kind in MeasurementDataFrame");
        }

        // Expand the measurement into flat columns.
        std::vector<Measurement> flat_fields;
        if (kind == DataKind::kScalar)
        {
            flat_fields.push_back(measurement);
        }
        else if (kind == DataKind::kVector)
        {
            const Index width = shape[0];
            flat_fields.reserve(static_cast<std::size_t>(width));
            for (Index i = 0; i < width; ++i)
                flat_fields.push_back(measurement.element_at(i));
        }
        else // Matrix
        {
            const Index rows = shape[0];
            const Index cols = shape[1];
            flat_fields.reserve(static_cast<std::size_t>(rows * cols));
            for (Index r = 0; r < rows; ++r)
                for (Index c = 0; c < cols; ++c)
                    flat_fields.push_back(measurement.element_at(r, c));
        }

        // Build the single row and store it directly (no lazy loading).
        row_.multi_index = {0};
        row_.fields = std::move(flat_fields);

        std::vector<DataFrameRow> rows;
        rows.push_back(row_);

        ConfigureStatic(std::move(all_headers), std::move(rows));
    }

    const DataFrameRow& MeasurementDataFrame::GetRow(Index /*row*/) const
    {
        return row_;
    }

    // =========================================================================
    // DataFrame: static factories
    // =========================================================================

    std::unique_ptr<DataFrame> DataFrame::FromBlock(const Block& block)
    {
        return std::unique_ptr<DataFrame>(new BlockDataFrame(block));
    }

    std::unique_ptr<DataFrame> DataFrame::FromDataArray(
        const DataArray& variable,
        std::string variable_name)
    {
        return std::unique_ptr<DataFrame>(
            new DataArrayDataFrame(variable, std::move(variable_name)));
    }

    std::unique_ptr<DataFrame> DataFrame::FromMeasurement(
        const Measurement& measurement,
        std::string name)
    {
        return std::unique_ptr<DataFrame>(
            new MeasurementDataFrame(measurement, std::move(name)));
    }

    // =========================================================================
    // DataFrame: header-rename hook
    // =========================================================================

    void DataFrame::UpdateVariableName(std::string /*variable_name*/)
    {
        // No-op: only DataArray-backed frames have a renameable dependent
        // column.  DataArrayDataFrame overrides this.
    }

} // namespace xdataset
