#pragma once
#include <string>
#include <unordered_set>
#include <vector>

#include "core/equation_context.h"
#include "environment.h"

namespace xequation
{
namespace rel_engine
{

// REL 方程上下文：包装 rel::Environment，实现 EquationContext 接口。
// 仿照 python/PythonEquationContext（其包装 pybind11::dict）。
//
// rel::Environment 提供 Define/Get/Remove/Clear/VariableNames（Remove/Clear
// 由上游 REL 提供），删除语义直接委托给 Environment，无需本地模拟。
class RelEquationContext : public EquationContext
{
  public:
    // Checks if key exists.
    bool Contains(const std::string &key) const override;

    // Gets value for the given key.  Null when not found or removed.
    EquationValue Get(const std::string &key) const override;

    // Sets value for the given key (requires rel::Value payload).
    void Set(const std::string &key, const EquationValue &value) override;

    // Removes the given key (delegates to rel::Environment::Remove).
    bool Remove(const std::string &key) override;

    // Clears all entries (delegates to rel::Environment::Clear).
    void Clear() override;

    // Returns all keys in the context.
    std::unordered_set<std::string> keys() const override;

    // Returns the number of entries.
    size_t size() const override;

    // Checks if the dictionary is empty.
    bool empty() const override;

    // 直接访问底层 rel::Environment（求值时传入）
    rel::Environment &env()
    {
        return env_;
    }
    const rel::Environment &env() const
    {
        return env_;
    }

    std::vector<std::string> GetBuiltinNames() const override;

    std::vector<std::string> GetSymbolNames() const override;

    std::string GetSymbolType(const std::string &symbol_name) const override;

    std::string GetTypeCategory(const std::string &type_name) const override;

  private:
    friend class RelEquationEngine;
    explicit RelEquationContext(const EquationEngineInfo &engine_info);
    ~RelEquationContext() noexcept = default;
    RelEquationContext(const RelEquationContext &) = delete;
    RelEquationContext &operator=(const RelEquationContext &) = delete;

    rel::Environment env_;
};

} // namespace rel_engine
} // namespace xequation
