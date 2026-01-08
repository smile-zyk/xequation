#pragma once
#include "core/equation_context.h"
#include "python_common.h"
#include <memory>

namespace xequation
{
namespace python
{
class PythonEquationContext : public EquationContext
{
  public:
    // Checks if key exists.
    bool Contains(const std::string &key) const override;

    // Gets value for the given key.
    Value Get(const std::string &key) const override;

    // Sets value for the given key.
    void Set(const std::string &key, const Value &value) override;

    // Removes the given key.
    bool Remove(const std::string &key) override;

    // Clears all entries.
    void Clear() override;

    // Returns all keys in the context.
    std::unordered_set<std::string> keys() const override;

    // Returns the number of entries.
    size_t size() const override;

    // Checks if the dictionary is empty.
    bool empty() const override;

    pybind11::dict &dict()
    {
        return *dict_;
    }
    const pybind11::dict &dict() const
    {
        return *dict_;
    }

    pybind11::dict builtin_dict() const;

    std::set<std::string> GetBuiltinNames() const override;

  private:
    friend class PythonEquationEngine;
    PythonEquationContext();
    ~PythonEquationContext() noexcept = default;
    PythonEquationContext(const PythonEquationContext &) = delete;
    PythonEquationContext &operator=(const PythonEquationContext &) = delete;

    PythonEquationContext(PythonEquationContext &&) noexcept = delete;
    PythonEquationContext &operator=(PythonEquationContext &&) noexcept = delete;
    std::unique_ptr<pybind11::dict> dict_;
    mutable std::set<std::string> builtin_names_cache_;
};
} // namespace python
} // namespace xequation