#pragma once

#include "function.h"
#include "value.h"

namespace rel {
namespace builtin {

// =============================================================================
//  Builtin operation C++ API
// =============================================================================

/// datasets() -- list every registered Dataset name.
Value Datasets();

/// default_dataset() -- current default Dataset name.
Value DefaultDataset();

/// variables() -- return empty list.
Value Variables();

/// what(x) -- inspect a Value: Dependency, Kind, Dimension, Data Shape, Data Type, Unit.
Value What(const Value& v);

/// indep(da, selector = Integer(1)) -- extract an independent variable from a DataArray.
/// selector: Integer = 1-based index, String = independent variable name.
Value Indep(const Value& da, const Value& selector);

/// output(da, variable_name = String("data")) -- write DataFrame to "<name>.csv".
/// Returns the absolute file path as a String Measurement.
Value Output(const Value& da, const Value& variable_name);

// =============================================================================
//  Library factory
// =============================================================================

/// Build the "builtin" function library.
FunctionLibrary MakeLibrary();

}  // namespace builtin
}  // namespace rel
