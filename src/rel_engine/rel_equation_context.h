#pragma once
#include <set>
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
// 注意：rel::Environment 提供 Define/Get/VariableNames，但没有 Remove。
// Remove 语义用内部 removed_ 集合模拟：被移除的名字不再出现在
// Contains/keys/Get 中，但绑定仍保留在 Environment 里（重新赋值会复活）。
class RelEquationContext : public EquationContext
{
  public:
    // Checks if key exists.
    bool Contains(const std::string &key) const override;

    // Gets value for the given key.  Null when not found or removed.
    EquationValue Get(const std::string &key) const override;

    // Sets value for the given key (requires rel::Value payload).
    void Set(const std::string &key, const EquationValue &value) override;

    // Removes the given key (marks removed; see class comment).
    bool Remove(const std::string &key) override;

    // Clears all entries.
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
    std::set<std::string> removed_;
};

} // namespace rel_engine
} // namespace xequation
