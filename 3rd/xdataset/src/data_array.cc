#include "data_array.h"
#include "data_series.h"
#include "dimension_spec.h"
#include "multi_dimension_spec.h"

#include <functional>
#include <map>
#include <stdexcept>
#include "ordered_map.h"
#include <vector>

namespace xdataset
{

    const char* DataArray::kSelf = "";

    namespace
    {

        /// Shared validation logic (assumes datas are already canonicalized).
        void validate_datas_internal(const ordered_map<std::string, DataSeries>& datas,
                                     const MultiDimensionSpec&                    multi_dimension_spec,
                                     DataArrayKind                                kind)
        {
            if (datas.empty())
                throw std::invalid_argument("DataArray: datas must not be empty");

            if (datas.rbegin()->first != DataArray::kSelf)
                throw std::invalid_argument(
                    "DataArray: last entry of datas must have key kSelf (empty string)");

            const std::size_t rank = multi_dimension_spec.rank();

            if (kind == DataArrayKind::kIndependent)
            {
                if (datas.size() != rank)
                    throw std::invalid_argument(
                        "DataArray: Independent datas count " + std::to_string(datas.size()) +
                        " must equal multi_dimension_spec rank " + std::to_string(rank));
            }
            else
            {
                if (datas.size() != rank + 1)
                    throw std::invalid_argument(
                        "DataArray: Dependent datas count " + std::to_string(datas.size()) +
                        " must equal rank + 1 (" + std::to_string(rank + 1) + ")");

                if (!multi_dimension_spec.empty())
                {
                    const std::size_t expected = multi_dimension_spec.compute_cell_count();
                    if (datas.rbegin()->second.size() != static_cast<Index>(expected))
                    {
                        throw std::invalid_argument(
                            "DataArray: dependent data size " +
                            std::to_string(datas.rbegin()->second.size()) +
                            " does not match multi_dimension_spec cell count " +
                            std::to_string(expected));
                    }
                }
            }
        }

    } // anonymous namespace

    void DataArray::Validate(const DataArrayCreateInfo& info)
    {
        validate_datas_internal(info.datas, info.multi_dimension_spec, info.kind);
    }

    const DataSeries& DataArray::data() const
    {
        auto it = datas_.end();
        --it;
        return it->second;
    }

    DataArray::DataArray(const DataArrayCreateInfo& info)
        : datas_(info.datas),
          multi_dimension_spec_(info.multi_dimension_spec),
          data_kind_(info.kind)
    {
        // Canonicalize then validate.  validate() canonicalizes its own copy
        // of the datas, so the member is handled independently.
        for (auto it = datas_.begin(); it != datas_.end(); ++it)
            it->second.canonicalize();
        validate_datas_internal(datas_, multi_dimension_spec_, data_kind_);
    }

    DataArray::DataArray(DataArrayCreateInfo&& info)
        : datas_(std::move(info.datas)),
          multi_dimension_spec_(std::move(info.multi_dimension_spec)),
          data_kind_(info.kind)
    {
        // info.datas has been moved; canonicalize and validate datas_ directly.
        for (auto it = datas_.begin(); it != datas_.end(); ++it)
            it->second.canonicalize();
        validate_datas_internal(datas_, multi_dimension_spec_, data_kind_);
    }

    DataArray::DataArray(const DataArray& other)
        : datas_(other.datas_),
          multi_dimension_spec_(other.multi_dimension_spec_),
          data_kind_(other.data_kind_)
    {
        // data_frame_cache_ intentionally left as nullptr.
    }

    DataArray& DataArray::operator=(const DataArray& other)
    {
        if (this != &other)
        {
            datas_ = other.datas_;
            multi_dimension_spec_ = other.multi_dimension_spec_;
            data_kind_ = other.data_kind_;
            data_frame_cache_.reset();
        }
        return *this;
    }

    std::vector<std::string> DataArray::indep_names() const
    {
        std::vector<std::string> names;
        const std::size_t rank = multi_dimension_spec_.rank();
        names.reserve(rank);
        for (const auto& item : datas_)
        {
            if (item.first == kSelf)
                break;  // kSelf is never an independent variable name
            names.push_back(item.first);
        }
        return names;
    }

    const DataFrame& DataArray::GetOrCreateDataFrame(const std::string& variable_name) const
    {
        if (!data_frame_cache_)
        {
            data_frame_cache_ = DataFrame::FromDataArray(*this, variable_name);
        }
        else
        {
            data_frame_cache_->UpdateVariableName(variable_name);
        }
        return *data_frame_cache_;
    }

    DataSeries DataArray::indep_data(Index index) const
    {
        if (index <= 0)
            throw std::invalid_argument("indep_data index must be 1-based and greater than 0");

        const std::size_t rank = multi_dimension_spec_.rank();
        if (static_cast<std::size_t>(index) > rank)
            throw std::out_of_range("indep_data index out of range");

        // index=1 -> last indep entry (innermost), index=rank -> first indep entry (outermost)
        const std::size_t target = rank - static_cast<std::size_t>(index);
        auto it = datas_.begin();
        std::advance(it, static_cast<std::ptrdiff_t>(target));

        // For Independent DataArrays, the innermost entry (index==1) is the
        // self/dimension data.  Return an index series [0, 1, 2, ...) instead
        // of the raw dimension values.
        if (data_kind_ == DataArrayKind::kIndependent && index == 1)
        {
            const DataSeries& raw = it->second;
            DataSeries idx = DataSeries::CreateScalar<int>(raw.size(), Unit(), 0);
            for (Index i = 0; i < raw.size(); ++i)
                idx.scalar_at<int>(i) = static_cast<int>(i);
            return idx;
        }

        return it->second;
    }

    DataSeries DataArray::indep_data(const std::string& name) const
    {
        if (data_kind_ == DataArrayKind::kDependent && name == kSelf)
            throw std::invalid_argument(
                "indep_data: kSelf is not an independent variable for Dependent DataArray");

        auto it = datas_.find(name);
        if (it == datas_.end())
            throw std::invalid_argument("indep_data name not found: " + name);

        // For Dependent, verify the entry is within the first rank entries.
        if (data_kind_ == DataArrayKind::kDependent)
        {
            const std::size_t rank = multi_dimension_spec_.rank();
            std::size_t pos = 0;
            for (auto dit = datas_.begin(); dit != datas_.end(); ++dit, ++pos)
            {
                if (dit->first == name)
                    break;
            }
            if (pos >= rank)
                throw std::invalid_argument(
                    "indep_data: '" + name + "' is not an independent variable");
        }

        return it->second;
    }

    DataArray DataArray::indep(Index index) const
    {
        if (index <= 0)
            throw std::invalid_argument("indep index must be 1-based and greater than 0");

        const std::size_t rank = multi_dimension_spec_.rank();
        if (static_cast<std::size_t>(index) > rank)
            throw std::out_of_range("indep index out of range");

        // index=1 -> last indep entry, index=rank -> first indep entry
        const std::size_t target = rank - static_cast<std::size_t>(index);

        DataArrayCreateInfo info;
        info.kind = DataArrayKind::kIndependent;

        // Copy outer independent entries.  indep_data(i) with i>1 never
        // triggers index generation, so it just copies the raw data.
        for (Index i = static_cast<Index>(rank); i > index; --i)
        {
            DataSeries ds = indep_data(i);
            auto it = datas_.begin();
            std::advance(it, static_cast<std::ptrdiff_t>(rank - static_cast<std::size_t>(i)));
            info.datas.emplace(it->first, std::move(ds));
        }

        // Self entry: innermost dimension.  indep_data() handles index
        // generation for Independent data automatically.
        info.datas.emplace(kSelf, indep_data(index));

        // Build result multi_dimension_spec from prefix dimensions.
        MultiDimensionSpec result_spec;
        for (std::size_t i = 0; i <= target; ++i)
            result_spec.add_dimension(multi_dimension_spec_.dims()[i]);
        info.multi_dimension_spec = result_spec;

        return DataArray(std::move(info));
    }

    DataArray DataArray::indep(const std::string& name) const
    {
        if (name.empty())
            throw std::invalid_argument("indep name must not be empty");

        std::size_t pos = 0;
        for (const auto& item : datas_)
        {
            // For Dependent, stop at rank (exclude kSelf).
            if (data_kind_ == DataArrayKind::kDependent && pos >= multi_dimension_spec_.rank())
                break;

            if (item.first == name)
            {
                const std::size_t rank = multi_dimension_spec_.rank();
                const Index index_1_based = static_cast<Index>(rank - pos);
                return indep(index_1_based);
            }
            ++pos;
        }

        throw std::invalid_argument("indep name not found: " + name);
    }

    DataArray DataArray::at(const std::vector<MultiIndexSelector>& selectors) const
    {
        if (data().data_kind() == DataKind::kScalar)
        {
            throw std::logic_error("at is invalid for scalar data");
        }

        const std::size_t ndim = data().data_shape().size();  // 1 for vector, 2 for matrix
        if (selectors.size() > ndim)
        {
            throw std::invalid_argument("too many selectors for at");
        }

        // Pad short selectors with Any so callers can omit trailing dimensions.
        std::vector<MultiIndexSelector> padded = selectors;
        while (padded.size() < ndim)
            padded.push_back(MultiIndexSelector::Any());

        DataArrayCreateInfo info;
        info.kind = data_kind_;
        info.datas = datas_;
        info.multi_dimension_spec = multi_dimension_spec_;

        if (data().data_kind() == DataKind::kVector)
        {
            const std::vector<Index> selected = padded[0].resolve(data().data_shape()[0]);
            info.datas[kSelf] = data().at(selected);
        }
        else
        {
            const std::vector<Index> selected_rows = padded[0].resolve(data().data_shape()[0]);
            const std::vector<Index> selected_cols = padded[1].resolve(data().data_shape()[1]);
            info.datas[kSelf] = data().at(selected_rows, selected_cols);
        }
        return DataArray(std::move(info));
    }

    DataArray DataArray::select(
        const std::vector<MultiIndexSelector>& selectors) const
    {
        const std::size_t rank = multi_dimension_spec_.rank();
        if (rank == 0)
        {
            throw std::logic_error("select requires non-empty dimensions");
        }

        if (selectors.size() > rank)
        {
            throw std::invalid_argument("selector count exceeds DataArray rank");
        }

        // `selectors` may be shorter than rank: normalize it by prepending Any and
        // keeping the user-provided selectors at the end, so short selectors only
        // constrain the trailing dimensions by default.
        std::vector<MultiIndexSelector> actual_selectors;
        actual_selectors.reserve(rank);
        if (selectors.size() < rank)
        {
            actual_selectors.insert(
                actual_selectors.end(), rank - selectors.size(), MultiIndexSelector::Any());
        }
        actual_selectors.insert(actual_selectors.end(), selectors.begin(), selectors.end());

        std::vector<bool> is_dim_retain(rank, true);
        for (std::size_t source_dim = 0; source_dim < rank; ++source_dim)
        {
            is_dim_retain[source_dim] = !actual_selectors[source_dim].is_equal();
        }

        // Independent DataArray: the last dimension (self) must never be eliminated,
        // even when the selector is Equal -- otherwise the result has no data.
        if (data_kind_ == DataArrayKind::kIndependent && rank > 0)
            is_dim_retain[rank - 1] = true;

        struct SelectionDimensionInformation
        {
            bool is_ragged = false;
            std::vector<Index> source_rows;
            std::vector<std::size_t> child_counts;
        };

        // key is source dimension index, value is SelectionDimensionInformation
        std::map<Index, SelectionDimensionInformation> selection_info;

        // for dependent DataArray, record source data selected row
        std::vector<Index> selected_row_indices;

        std::function<void(Index, Index)> walk = [&](Index dim_idx, Index parent_flat)
        {
            if (dim_idx == static_cast<Index>(rank))
            {
                selected_row_indices.push_back(parent_flat);
                return;
            }

            const DimensionSpec& dim = multi_dimension_spec_.dim(dim_idx);
            std::size_t width = 0;
            if (dim.is_regular())
            {
                width = dim.regular_size();
            }
            else
            {
                width = dim.child_width(parent_flat);
            }

            const std::vector<Index> selected_children =
                actual_selectors[static_cast<std::size_t>(dim_idx)].resolve(static_cast<Index>(width));

            selection_info[dim_idx].child_counts.push_back(selected_children.size());
            selection_info[dim_idx].is_ragged = !dim.is_regular();

            if (selection_info[dim_idx].is_ragged)
            {
                for (Index child : selected_children)
                {
                    Index start = 0;
                    Index end = 0;
                    dim.child_range(parent_flat, start, end);
                    Index source_row = start + child;
                    selection_info[dim_idx].source_rows.push_back(source_row);
                }
            }
            else
            {
                if (selection_info[dim_idx].source_rows.empty())
                {
                    selection_info[dim_idx].source_rows = selected_children;
                }
            }

            for (Index child : selected_children)
            {
                Index current_flat = 0;
                if (dim.is_regular())
                {
                    const Index size = static_cast<Index>(dim.regular_size());
                    current_flat = parent_flat * size + child;
                }
                else
                {
                    Index start = 0;
                    Index end = 0;
                    dim.child_range(parent_flat, start, end);
                    (void)end;
                    current_flat = start + child;
                }

                walk(dim_idx + 1, current_flat);
            }
        };

        walk(0, 0);

        MultiDimensionSpec selected_multi_dim;
        for (std::size_t i = 0; i < rank; ++i)
        {
            if (!is_dim_retain[i])
            {
                continue;
            }

            const std::vector<std::size_t>& counts = selection_info[static_cast<Index>(i)].child_counts;
            if (counts.empty())
            {
                selected_multi_dim.add_regular(0);
                continue;
            }

            if (selection_info[static_cast<Index>(i)].is_ragged)
            {
                if (counts.size() == 1)
                {
                    selected_multi_dim.add_regular(counts.front());
                }
                else
                {
                    selected_multi_dim.add_ragged(counts);
                }
            }
            else
            {
                selected_multi_dim.add_regular(counts.front());
            }
        }

        DataArrayCreateInfo info;
        info.kind = data_kind_;
        info.multi_dimension_spec = selected_multi_dim;

        // Iterate datas_ in order and select from each entry.
        // Independent: each position maps 1:1 to a dimension.
        // Dependent:   first rank positions are indep dims; last (kSelf) is
        //              the expanded dependent data, selected by flat index.
        std::size_t idx = 0;
        for (auto it = datas_.begin(); it != datas_.end(); ++it, ++idx)
        {
            const bool is_self = (idx == datas_.size() - 1);   // kSelf entry

            if (is_self && data_kind_ == DataArrayKind::kDependent)
            {
                // Dependent kSelf: select by flat row indices.
                DataSeries sel(data().data_type(), data().data_shape());
                for (Index r : selected_row_indices)
                    sel.append_from(data(), r);
                info.datas.emplace(kSelf, std::move(sel));
            }
            else
            {
                // Dimension data (Independent all entries, Dependent first rank).
                if (!is_dim_retain[idx])
                    continue;

                const auto& src_rows = selection_info[static_cast<Index>(idx)].source_rows;
                const DataSeries& src_series = it->second;
                DataSeries sel(src_series.data_type(), src_series.data_shape());
                for (Index r : src_rows)
                    sel.append_from(src_series, r);

                std::string key = is_self ? std::string(kSelf) : it->first;
                info.datas.emplace(std::move(key), std::move(sel));
            }
        }

        // If only kSelf remains and the original was Dependent, demote to
        // Independent because a Dependent DataArray requires independent
        // variables as dependencies.
        if (data_kind_ == DataArrayKind::kDependent && info.datas.size() == 1)
        {
            info.kind = DataArrayKind::kIndependent;
            const Index data_size = info.datas[kSelf].size();
            info.multi_dimension_spec =
                MultiDimensionSpec().add_regular(static_cast<std::size_t>(data_size));
        }

        return DataArray(std::move(info));
    }

    void DataArray::for_each_indep_group(
        Index                                          indep_index,
        const MultiDimensionSpec::DimGroupVisitor&     visitor) const
    {
        if (indep_index <= 0)
            throw std::invalid_argument("for_each_indep_group: indep_index must be 1-based and greater than 0");

        const std::size_t rank = multi_dimension_spec_.rank();
        if (rank == 0)
            throw std::logic_error("for_each_indep_group: DataArray has no dimensions");

        if (static_cast<std::size_t>(indep_index) > rank)
            throw std::out_of_range("for_each_indep_group: indep_index out of range");

        // Convert: indep_index=1 (innermost) -> spec dim rank-1;
        //          indep_index=rank (outermost) -> spec dim 0.
        const Index spec_dim = static_cast<Index>(rank) - indep_index;
        multi_dimension_spec_.for_each_group_at_dim(spec_dim, visitor);
    }

    void DataArray::for_each_leaf_row(
        const MultiDimensionSpec::LeafRowVisitor& visitor) const
    {
        multi_dimension_spec_.for_each_leaf_row(visitor);
    }

    void DataArray::for_each_leaf_row(
        const MultiDimensionSpec::LeafRowVisitor& visitor,
        Index start_flat_row, Index end_flat_row) const
    {
        multi_dimension_spec_.for_each_leaf_row(visitor, start_flat_row, end_flat_row);
    }

    // Static factory methods

    DataArray DataArray::CreateIndependent(
        DataSeries data)
    {
        const std::size_t size = data.size();
        DataArrayCreateInfo vinfo;
        vinfo.datas[kSelf] = std::move(data);
        vinfo.multi_dimension_spec = MultiDimensionSpec().add_regular(size);
        vinfo.kind = DataArrayKind::kIndependent;
        return DataArray(std::move(vinfo));
    }

    DataArray DataArray::CreateDependent(
        DataSeries data,
        const ordered_map<std::string, const DataArray*>& indep_variables)
    {
        if (indep_variables.empty())
        {
            throw std::invalid_argument(
                "CreateDependent: indep_variables must not be empty");
        }

        MultiDimensionSpec spec;

        for (const auto& item : indep_variables)
        {
            const std::string& var_name = item.first;
            const DataArray*   var      = item.second;
            if (!var)
            {
                throw std::invalid_argument(
                    "CreateDependent: null indep_variable in list");
            }
            if (var->data_kind() != DataArrayKind::kIndependent)
            {
                throw std::invalid_argument(
                    "CreateDependent: DataArray is not an independent DataArray");
            }

            const std::vector<DimensionSpec>& dims = var->multi_dimension_spec().dims();
            if (dims.empty())
            {
                throw std::logic_error(
                    "CreateDependent: independent DataArray has no dimensions");
            }
            spec.add_dimension(dims.back());   // validates parent count for ragged
        }

        DataArrayCreateInfo vinfo;
        // Collect indep variable data first (ordered by insertion into spec).
        for (const auto& item : indep_variables)
        {
            vinfo.datas[item.first] = item.second->data();
        }
        // Self data (dependent) goes last.
        vinfo.datas[kSelf] = std::move(data);
        vinfo.multi_dimension_spec = std::move(spec);
        vinfo.kind = DataArrayKind::kDependent;
        return DataArray(std::move(vinfo));
    }

// =========================================================================
//  DataArray -- set_data / clone
// =========================================================================

void DataArray::set_data(DataSeries new_self)
{
    // For Dependent, validate that the new series size matches the cell count.
    if (data_kind_ == DataArrayKind::kDependent && !multi_dimension_spec_.empty())
    {
        const std::size_t expected = multi_dimension_spec_.compute_cell_count();
        if (new_self.size() != static_cast<Index>(expected))
        {
            throw std::invalid_argument(
                "set_data: new series size " +
                std::to_string(new_self.size()) +
                " does not match multi_dimension_spec cell count " +
                std::to_string(expected));
        }
    }

    // Canonicalize the new series for consistency with DataArray invariants.
    new_self.canonicalize();

    // Replace the last entry (kSelf) in the ordered map.
    datas_[kSelf] = std::move(new_self);

    // Invalidate cached DataFrame.
    data_frame_cache_.reset();
}

void DataArray::set_data(Index row, Measurement value)
{
    if (row < 0 || static_cast<std::size_t>(row) >= data().size())
        throw std::out_of_range("set_data row out of range");

    if (value.data_kind() != data().data_kind())
        throw std::invalid_argument("set_data: measurement kind does not match series");

    if (value.data_type() != data().data_type())
        throw std::invalid_argument("set_data: measurement dtype does not match series");

    if (value.shape() != data().data_shape())
        throw std::invalid_argument("set_data: measurement shape does not match series");

    DataSeries& self_series = datas_[kSelf];

    if (value.data_kind() == DataKind::kScalar)
    {
        if (value.data_type() == DataType::kReal)
            self_series.scalar_at<double>(row) = value.as_scalar<double>();
        else if (value.data_type() == DataType::kInteger)
            self_series.scalar_at<int>(row) = value.as_scalar<int>();
        else if (value.data_type() == DataType::kComplex)
            self_series.scalar_at<std::complex<double>>(row) = value.as_scalar<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            self_series.scalar_at<std::string>(row) = value.as_scalar<std::string>();
        else
            throw std::invalid_argument("set_data: unsupported scalar dtype");
    }
    else if (value.data_kind() == DataKind::kVector)
    {
        if (value.data_type() == DataType::kReal)
            self_series.vector_at<double>(row) = value.as_vector<double>();
        else if (value.data_type() == DataType::kInteger)
            self_series.vector_at<int>(row) = value.as_vector<int>();
        else if (value.data_type() == DataType::kComplex)
            self_series.vector_at<std::complex<double>>(row) = value.as_vector<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            self_series.vector_at<std::string>(row) = value.as_vector<std::string>();
        else
            throw std::invalid_argument("set_data: unsupported vector dtype");
    }
    else
    {
        if (value.data_type() == DataType::kReal)
            self_series.matrix_at<double>(row) = value.as_matrix<double>();
        else if (value.data_type() == DataType::kInteger)
            self_series.matrix_at<int>(row) = value.as_matrix<int>();
        else if (value.data_type() == DataType::kComplex)
            self_series.matrix_at<std::complex<double>>(row) = value.as_matrix<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            self_series.matrix_at<std::string>(row) = value.as_matrix<std::string>();
        else
            throw std::invalid_argument("set_data: unsupported matrix dtype");
    }

    data_frame_cache_.reset();
}

void DataArray::set_indep_data(DataSeries new_series)
{
    if (datas_.size() <= 1)
        throw std::invalid_argument("set_indep_data: no independent data series available");

    auto it = datas_.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(datas_.size() - 2));

    if (new_series.size() != it->second.size())
    {
        throw std::invalid_argument(
            "set_indep_data: new series size " + std::to_string(new_series.size()) +
            " does not match existing series size " + std::to_string(it->second.size()));
    }

    new_series.canonicalize();
    it->second = std::move(new_series);
    data_frame_cache_.reset();
}

void DataArray::set_indep_data(Index indep_index, DataSeries new_series)
{
    if (indep_index <= 0)
        throw std::invalid_argument("indep_index must be 1-based and greater than 0");

    const std::size_t rank = multi_dimension_spec_.rank();
    if (static_cast<std::size_t>(indep_index) > rank)
        throw std::out_of_range("indep_index out of range");

    if (indep_index == 1 && data_kind_ == DataArrayKind::kIndependent)
        throw std::invalid_argument(
            "set_indep_data: indep_index=1 targets kSelf for Independent DataArray; use set_data() instead");

    const std::size_t target = rank - static_cast<std::size_t>(indep_index);
    auto it = datas_.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(target));

    if (new_series.size() != it->second.size())
    {
        throw std::invalid_argument(
            "set_indep_data: new series size " + std::to_string(new_series.size()) +
            " does not match existing series size " + std::to_string(it->second.size()));
    }

    new_series.canonicalize();
    it->second = std::move(new_series);
    data_frame_cache_.reset();
}

void DataArray::set_indep_data(const std::string& indep_name, DataSeries new_series)
{
    if (indep_name == kSelf)
        throw std::invalid_argument(
            "set_indep_data: cannot modify kSelf; use set_data() instead");

    auto it = datas_.find(indep_name);
    if (it == datas_.end())
        throw std::invalid_argument("indep_data name not found: " + indep_name);

    if (new_series.size() != it->second.size())
    {
        throw std::invalid_argument(
            "set_indep_data: new series size " + std::to_string(new_series.size()) +
            " does not match existing series size " + std::to_string(it->second.size()));
    }

    new_series.canonicalize();
    it->second = std::move(new_series);
    data_frame_cache_.reset();
}

void DataArray::set_indep_data(Index indep_index, Index row, Measurement value)
{
    if (indep_index <= 0)
        throw std::invalid_argument("indep_index must be 1-based and greater than 0");

    const std::size_t rank = multi_dimension_spec_.rank();
    if (static_cast<std::size_t>(indep_index) > rank)
        throw std::out_of_range("indep_index out of range");

    if (indep_index == 1 && data_kind_ == DataArrayKind::kIndependent)
        throw std::invalid_argument(
            "set_indep_data: indep_index=1 targets kSelf for Independent DataArray; use set_data() instead");

    const std::size_t target = rank - static_cast<std::size_t>(indep_index);
    auto it = datas_.begin();
    std::advance(it, static_cast<std::ptrdiff_t>(target));

    if (row < 0 || static_cast<std::size_t>(row) >= it->second.size())
        throw std::out_of_range("set_indep_data row out of range");

    if (value.data_kind() != it->second.data_kind())
        throw std::invalid_argument("set_indep_data: measurement kind does not match series");

    if (value.data_type() != it->second.data_type())
        throw std::invalid_argument("set_indep_data: measurement dtype does not match series");

    if (value.shape() != it->second.data_shape())
        throw std::invalid_argument("set_indep_data: measurement shape does not match series");

    DataSeries& target_series = it->second;

    if (value.data_kind() == DataKind::kScalar)
    {
        if (value.data_type() == DataType::kReal)
            target_series.scalar_at<double>(row) = value.as_scalar<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.scalar_at<int>(row) = value.as_scalar<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.scalar_at<std::complex<double>>(row) = value.as_scalar<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.scalar_at<std::string>(row) = value.as_scalar<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported scalar dtype");
    }
    else if (value.data_kind() == DataKind::kVector)
    {
        if (value.data_type() == DataType::kReal)
            target_series.vector_at<double>(row) = value.as_vector<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.vector_at<int>(row) = value.as_vector<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.vector_at<std::complex<double>>(row) = value.as_vector<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.vector_at<std::string>(row) = value.as_vector<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported vector dtype");
    }
    else
    {
        if (value.data_type() == DataType::kReal)
            target_series.matrix_at<double>(row) = value.as_matrix<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.matrix_at<int>(row) = value.as_matrix<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.matrix_at<std::complex<double>>(row) = value.as_matrix<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.matrix_at<std::string>(row) = value.as_matrix<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported matrix dtype");
    }

    data_frame_cache_.reset();
}

void DataArray::set_indep_data(const std::string& indep_name, Index row, Measurement value)
{
    if (indep_name == kSelf)
        throw std::invalid_argument(
            "set_indep_data: cannot modify kSelf; use set_data() instead");

    auto it = datas_.find(indep_name);
    if (it == datas_.end())
        throw std::invalid_argument("indep_data name not found: " + indep_name);

    if (row < 0 || static_cast<std::size_t>(row) >= it->second.size())
        throw std::out_of_range("set_indep_data row out of range");

    if (value.data_kind() != it->second.data_kind())
        throw std::invalid_argument("set_indep_data: measurement kind does not match series");

    if (value.data_type() != it->second.data_type())
        throw std::invalid_argument("set_indep_data: measurement dtype does not match series");

    if (value.shape() != it->second.data_shape())
        throw std::invalid_argument("set_indep_data: measurement shape does not match series");

    DataSeries& target_series = it->second;

    if (value.data_kind() == DataKind::kScalar)
    {
        if (value.data_type() == DataType::kReal)
            target_series.scalar_at<double>(row) = value.as_scalar<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.scalar_at<int>(row) = value.as_scalar<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.scalar_at<std::complex<double>>(row) = value.as_scalar<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.scalar_at<std::string>(row) = value.as_scalar<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported scalar dtype");
    }
    else if (value.data_kind() == DataKind::kVector)
    {
        if (value.data_type() == DataType::kReal)
            target_series.vector_at<double>(row) = value.as_vector<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.vector_at<int>(row) = value.as_vector<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.vector_at<std::complex<double>>(row) = value.as_vector<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.vector_at<std::string>(row) = value.as_vector<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported vector dtype");
    }
    else
    {
        if (value.data_type() == DataType::kReal)
            target_series.matrix_at<double>(row) = value.as_matrix<double>();
        else if (value.data_type() == DataType::kInteger)
            target_series.matrix_at<int>(row) = value.as_matrix<int>();
        else if (value.data_type() == DataType::kComplex)
            target_series.matrix_at<std::complex<double>>(row) = value.as_matrix<std::complex<double>>();
        else if (value.data_type() == DataType::kString)
            target_series.matrix_at<std::string>(row) = value.as_matrix<std::string>();
        else
            throw std::invalid_argument("set_indep_data: unsupported matrix dtype");
    }

    data_frame_cache_.reset();
}

DataArray DataArray::clone() const
{
    return DataArray(*this);
}

} // namespace xdataset
