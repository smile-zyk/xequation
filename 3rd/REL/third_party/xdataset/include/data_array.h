#ifndef DATA_ARRAY_H
#define DATA_ARRAY_H

#include <memory>
#include <string>
#include "ordered_map.h"
#include <vector>

#include "data_series.h"
#include "data_frame.h"
#include "multi_dimension_spec.h"
#include "multi_index_selector.h"

namespace xdataset
{
    using DataSeriesMap = ordered_map<std::string, DataSeries>;

    enum class DataArrayKind
    {
        kDependent,
        kIndependent
    };

    struct DataArrayCreateInfo
    {
        /// Unified storage for independent variable data and self data.
        /// The last entry (key = kSelf = "") is always the self data.
        /// For Independent DataArrays, the number of entries equals
        /// multi_dimension_spec.rank(), and all entries (including the last)
        /// are independent dimension data stored in raw (un-expanded) form.
        /// For Dependent DataArrays, the number of entries equals
        /// multi_dimension_spec.rank() + 1: the first rank entries are
        /// independent variable data (expanded), and the last (kSelf) is
        /// the dependent data (also expanded).
        DataSeriesMap datas;

        MultiDimensionSpec multi_dimension_spec;
        DataArrayKind kind = DataArrayKind::kDependent;
    };

    class XDATASET_API DataArray
    {
    public:
        /// Key used in datas_ for the self-reference entry.
        /// Always positioned last in the ordered map.
        static const char* kSelf;

        explicit DataArray(const DataArrayCreateInfo& info);
        explicit DataArray(DataArrayCreateInfo&& info);

        /// Validate a DataArrayCreateInfo: checks datas structure against
        /// multi_dimension_spec and kind.  Throws std::invalid_argument on error.
        /// The caller must canonicalize info.datas before calling.
        static void Validate(const DataArrayCreateInfo& info);

        // Copy: deep-copies data, resets the data_frame cache.
        DataArray(const DataArray& other);
        DataArray& operator=(const DataArray& other);

        // Move: default (all members are movable).
        DataArray(DataArray&&) = default;
        DataArray& operator=(DataArray&&) = default;

        /// Self data -- the last entry in datas_ (key = kSelf).
        /// For Independent: raw (un-expanded) dimension data of the last dimension.
        /// For Dependent:   the dependent variable data (already expanded).
        const DataSeries& data() const;

        /// Replace the self DataSeries (the last entry in datas_ with key = kSelf).
        /// For Dependent DataArrays, the new series must have the same size as
        /// multi_dimension_spec().compute_cell_count().  The data_frame cache is
        /// invalidated.  The new series must be canonicalized before replacement.
        /// This mutates the DataArray in place.
        void set_data(DataSeries new_self);

        /// Replace the value at a specific row in the self data series.
        void set_data(Index row, Measurement value);

        /// Replace the last independent data series wholesale.
        /// The new series length must match the existing series length so the
        /// multi_dimension_spec and downstream indep() semantics remain valid.
        void set_indep_data(DataSeries new_series);

        /// Replace an independent data series wholesale by index.
        void set_indep_data(Index indep_index, DataSeries new_series);

        /// Replace an independent data series wholesale by name.
        void set_indep_data(const std::string& indep_name, DataSeries new_series);

        /// Replace the value at a specific row in an independent data series by index.
        void set_indep_data(Index indep_index, Index row, Measurement value);

        /// Replace the value at a specific row in an independent data series by name.
        void set_indep_data(const std::string& indep_name, Index row, Measurement value);

        /// Return a deep copy of this DataArray.
        DataArray clone() const;

        /// Apply a transformation callback to the self DataSeries and return a
        /// new DataArray with the same independent dimensions / multi_dimension_spec
        /// but the transformed self data.  Delegates to DataSeries::transform.
        ///
        /// The callback receives a Measurement per row and returns a new Measurement.
        /// The output type (dtype/kind/shape) may differ from the input.
        ///
        /// Example:
        ///   auto da_squared = da.transform([](const Measurement& m) {
        ///       return Measurement(m.as_scalar<double>() * m.as_scalar<double>(), m.unit());
        ///   });
        template <typename Func>
        DataArray transform(Func&& callback) const
        {
            DataSeries new_self = data().transform(std::forward<Func>(callback));
            DataArray result(*this);
            result.set_data(std::move(new_self));
            return result;
        }

        const MultiDimensionSpec& multi_dimension_spec() const
        {
            return multi_dimension_spec_;
        }

        DataArrayKind data_kind() const
        {
            return data_kind_;
        }

        /// Number of elements per cell: delegates to data().element_count()
        Index element_count() const
        {
            return data().element_count();
        }

        const DataFrame& GetOrCreateDataFrame(const std::string& variable_name = "data") const;

        /// Full unified data map. The last entry is always kSelf.
        const DataSeriesMap& datas() const
        {
            return datas_;
        }

        /// Ordered names of independent variables.
        std::vector<std::string> indep_names() const;

        /// Independent data by index (1-based, innermost-first).
        /// Returns a copy of the data series.  For Independent DataArrays
        /// with index=1 (innermost / self), returns an index series
        /// [0, 1, 2, ...) instead of the raw data.
        DataSeries indep_data(Index index) const;

        /// Independent data by name.
        DataSeries indep_data(const std::string& name) const;


        DataArray indep(Index index = 1) const;

        DataArray indep(const std::string& name) const;

        DataArray at(const std::vector<MultiIndexSelector>& selectors) const;

        DataArray select(const std::vector<MultiIndexSelector>& selectors) const;

        /// Visit groups at a given independent dimension level.  The index
        /// follows the same 1-based, innermost-first convention as indep()
        /// and indep_data(): 1 = innermost dimension, rank = outermost.
        /// Delegates to MultiDimensionSpec::for_each_group_at_dim() after
        /// converting to the spec-level 0-based index.
        void for_each_indep_group(
            Index                          indep_index,
            const MultiDimensionSpec::DimGroupVisitor& visitor) const;

        /// Visit every leaf row in row-major order.
        /// Delegates to MultiDimensionSpec::for_each_leaf_row().
        void for_each_leaf_row(
            const MultiDimensionSpec::LeafRowVisitor& visitor) const;

        /// Visit leaf rows whose flat index falls in [start_flat_row, end_flat_row).
        void for_each_leaf_row(
            const MultiDimensionSpec::LeafRowVisitor& visitor,
            Index start_flat_row, Index end_flat_row) const;

        // Standalone independent variable (no prior independents).
        static DataArray CreateIndependent(
            DataSeries data);

        // Dependent variable with named independent DataArray objects.
        static DataArray CreateDependent(
            DataSeries data,
            const ordered_map<std::string, const DataArray*>& indep_variables);

    private:

        /// Unified data storage.  The last entry (key = kSelf) is always the
        /// self data; preceding entries are independent dimension / variable
        /// data.
        DataSeriesMap datas_;

        MultiDimensionSpec multi_dimension_spec_;
        DataArrayKind       data_kind_;
        mutable std::unique_ptr<DataFrame> data_frame_cache_;
    };
} // namespace xdataset

#endif // DATA_ARRAY_H
