#pragma once

#include "function.h"

#include <string>
#include <vector>
#include <utility>

namespace rel
{
    /// A named collection of functions, registerable in one call
    /// (see Environment::RegisterLibrary).
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
