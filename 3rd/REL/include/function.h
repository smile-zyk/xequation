#pragma once

#include "rel_api.h"
#include "value.h"  // rel::Value

#include "ordered_map.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace rel
{
    // =====================================================================
    //  Function registry -- host-registered callable functions
    // =====================================================================
    //
    //  A registered function may declare parameters with optional default
    //  values.  Call sites may omit any parameter slot (per REL's default
    //  argument slots, e.g. `func(a, , c)`), and omitted slots are filled
    //  with the declared default.  Defaults do not need to be trailing:
    //  any parameter that has a default may be skipped at the call site.
    //
    //  Slot resolution (evaluating explicit arguments, filling omitted slots
    //  with defaults) is done inside Function::Invoke -- the Evaluator
    //  packs explicit call-site arguments into an ArgMap and delegates
    //  everything else to Invoke().
    //
    //  Some parameters have a "computed default" -- a value that cannot be
    //  supplied as a static constant, but must be produced at resolve time
    //  from the already-resolved parameters.  Computed defaults are
    //  registered via ComputedParam().

    /// Named-argument map: param names to resolved Values, in declaration order.
    using ArgMap = ordered_map<std::string, Value>;

    /// Native implementation of a registered REL function.
    typedef std::function<Value(const ArgMap&)> NativeFunction;

    /// One parameter of a function; may carry a default value.
    struct FunctionParam
    {
        std::string         name;
        bool                has_default          = false;
        bool                has_computed_default = false;
        Value               default_value;
        NativeFunction       computed_default;

        FunctionParam() = default;

        /// Required parameter (no default).
        explicit FunctionParam(std::string name_value)
            : name(std::move(name_value))
        {}

        /// Parameter with a fixed default value.
        FunctionParam(std::string name_value, Value default_value_value)
            : name(std::move(name_value))
            , has_default(true)
            , default_value(std::move(default_value_value))
        {}

        /// Parameter whose default is computed at resolve time.
        FunctionParam(std::string name_value, NativeFunction computed_default_value)
            : name(std::move(name_value))
            , has_computed_default(true)
            , computed_default(std::move(computed_default_value))
        {}
    };

    /// Convenience: a required parameter.
    inline FunctionParam Param(std::string name)
    {
        return FunctionParam(std::move(name));
    }

    /// Convenience: a parameter with a fixed default value.
    inline FunctionParam Param(std::string name, Value default_value)
    {
        return FunctionParam(std::move(name), std::move(default_value));
    }

    /// Convenience: a parameter whose default is computed at resolve time.
    inline FunctionParam ComputedParam(std::string name, NativeFunction fn)
    {
        return FunctionParam(std::move(name), std::move(fn));
    }

    /// A user-registered function with parameter defaults.
    class Function
    {
    public:
        /// Named-argument map: param names to resolved Values, in declaration order.
        using ArgMap = ordered_map<std::string, Value>;
        Function() = default;

        Function(std::string name_value,
                 std::vector<FunctionParam> params_value,
                 NativeFunction impl_value)
            : name_(std::move(name_value))
            , params_(std::move(params_value))
            , impl_(std::move(impl_value))
        {}

        const std::string&              name()   const { return name_; }
        const std::vector<FunctionParam>& params() const { return params_; }
        const NativeFunction&           impl()   const { return impl_; }

        /// Number of declared parameters.
        std::size_t arity() const { return params_.size(); }

        /// True when the parameter at `index` declares a default value
        /// (either static or computed).
        bool HasDefault(std::size_t index) const
        {
            return index < params_.size()
                && (params_[index].has_default || params_[index].has_computed_default);
        }

        /// True when the parameter at `index` has a computed default.
        bool IsComputedDefault(std::size_t index) const
        {
            return index < params_.size() && params_[index].has_computed_default;
        }

        /// Invoke the implementation.
        ///
        /// `user_args` contains only the explicitly-provided key-value pairs.
        /// Missing parameters are filled from static or computed defaults
        /// (in declaration order) before calling impl_.  Throws when a
        /// required parameter is missing.
        REL_API Value Invoke(const ArgMap& user_args) const;

    private:
        std::string                name_;
        std::vector<FunctionParam> params_;
        NativeFunction             impl_;
    };

    // =====================================================================
    //  FunctionLibrary -- a named batch of functions
    // =====================================================================
    //
    //  A collection of functions that can be registered in one call
    //  (see Environment::RegisterLibrary).  Header-only; used by C++
    //  extensions to hand a whole library to the host.

    class FunctionLibrary
    {
    public:
        FunctionLibrary() = default;

        explicit FunctionLibrary(std::string name)
            : name_(std::move(name))
        {}

        const std::string&            name()      const { return name_; }
        const std::vector<Function>& functions() const { return functions_; }

        std::size_t size()  const { return functions_.size(); }
        bool        empty() const { return functions_.empty(); }

        /// Append one function.
        FunctionLibrary& Add(Function fn)
        {
            functions_.push_back(std::move(fn));
            return *this;
        }

        /// Build and append one function from its parts.
        FunctionLibrary& Add(const std::string& name,
                             std::vector<FunctionParam> params,
                             NativeFunction impl)
        {
            return Add(Function(name, std::move(params), std::move(impl)));
        }

    private:
        std::string           name_;
        std::vector<Function> functions_;
    };

}  // namespace rel

