#pragma once
#include <string>
#include <vector>

#include <boost/compute/detail/lru_cache.hpp>

#include "core/equation_common.h"
#include "expr.h"  // rel::ExprPtr / rel::ExprVisitor

namespace xequation
{
namespace rel_engine
{

// REL 语句解析器：仿照 python/PythonParser 的接口。
// 依赖提取基于 REL 的语法树（rel::Parse -> ExprPtr）：
//   - 赋值语句在文本层拆分（与 rel::Exec 的 find_binding_eq 一致）；
//   - 右侧表达式用 rel::Parse 得到 AST，经 ExprVisitor 遍历收集
//     ReferenceExpr（引用路径），并跳过函数调用名与内置常量。
class RelParser
{
  public:
    RelParser() = default;
    ~RelParser() = default;

    RelParser(const RelParser &) = delete;
    RelParser &operator=(const RelParser &) = delete;

    // 按 ';' 或换行拆分语句
    std::vector<std::string> SplitStatements(const std::string &code);

    // 解析多条语句
    ParseResult ParseStatements(const std::string &code);

    // 解析单条语句（赋值 -> kVariable；否则 kExpression）
    ParseResult ParseSingleStatement(const std::string &code);

    // 解析单个表达式，提取依赖
    ParseResult ParseExpression(const std::string &code);

    size_t GetParseResultCacheSize() const { return parse_result_cache_.size(); }

  private:
    // 遍历表达式 AST，提取引用路径依赖（去重保序）
    std::vector<std::string> ExtractDependencies(const std::string &expression) const;
    std::vector<std::string> ExtractDependencies(const rel::ExprPtr &expr) const;

    // 仿 rel::Exec：找顶层绑定 '='（跳过 ==, !=, <=, >=）
    static std::size_t FindBindingEq(const std::string &line);
    static std::string Trim(const std::string &s);
    static bool IsValidIdentifier(const std::string &name);

    void EvictLRU();

  private:
    static constexpr size_t max_cache_size_ = 50;
    boost::compute::detail::lru_cache<std::string, ParseResult> parse_result_cache_{max_cache_size_};
};

} // namespace rel_engine
} // namespace xequation
