#include "rel_parser.h"

#include <cctype>
#include <set>

#include "environment.h"
#include "expr.h"
#include "rel.h"

namespace xequation
{
namespace rel_engine
{

namespace
{

// ---------------------------------------------------------------------------
// 依赖收集 visitor：遍历 rel::Expr 语法树，收集所有"读取"的引用路径。
//
//  与 Python 引擎的 _DependencyVisitor 对齐：
//   - ReferenceExpr 收集引用路径（多段路径收集所有前缀，如 a.b -> a、a.b）；
//   - CallExpr 的 callee 若是单段标识符且注册在函数表 -> 函数调用，
//     callee 不是依赖（只收参数）；否则按矩阵索引处理（callee 是依赖）；
//   - 单段引用若是内置常量 -> 不是依赖；
//   - 叶子节点（数字/布尔/字符串/空范围）无依赖。
// ---------------------------------------------------------------------------
class RelDependencyVisitor : public rel::ExprVisitor
{
  public:
    explicit RelDependencyVisitor(std::vector<std::string> &out) : out_(out) {}

    void visit_number(const rel::NumberExpr &) override {}
    void visit_boolean(const rel::BooleanExpr &) override {}
    void visit_string(const rel::StringExpr &) override {}
    void visit_null_range(const rel::NullRangeExpr &) override {}

    void visit_reference(const rel::ReferenceExpr &expr) override
    {
        CollectReference(expr);
    }

    void visit_unary(const rel::UnaryExpr &expr) override
    {
        if (expr.operand)
            expr.operand->accept(*this);
    }

    void visit_binary(const rel::BinaryExpr &expr) override
    {
        if (expr.left)
            expr.left->accept(*this);
        if (expr.right)
            expr.right->accept(*this);
    }

    void visit_logical(const rel::LogicalExpr &expr) override
    {
        if (expr.left)
            expr.left->accept(*this);
        if (expr.right)
            expr.right->accept(*this);
    }

    void visit_conditional(const rel::ConditionalExpr &expr) override
    {
        if (expr.condition)
            expr.condition->accept(*this);
        if (expr.then_branch)
            expr.then_branch->accept(*this);
        if (expr.else_branch)
            expr.else_branch->accept(*this);
    }

    void visit_if(const rel::IfExpr &expr) override
    {
        for (const auto &branch : expr.branches)
        {
            if (branch.condition)
                branch.condition->accept(*this);
            if (branch.value)
                branch.value->accept(*this);
        }
        if (expr.else_value)
            expr.else_value->accept(*this);
    }

    void visit_call(const rel::CallExpr &expr) override
    {
        // 函数调用 vs 矩阵索引：callee 是单段标识符且注册为函数 -> 函数调用
        const rel::ReferenceExpr *ref =
            dynamic_cast<const rel::ReferenceExpr *>(expr.callee.get());
        const bool is_function_call =
            ref && ref->segments.size() == 1 &&
            rel::Environment::HasFunction(ref->segments[0].name);

        if (!is_function_call && expr.callee)
        {
            expr.callee->accept(*this);  // 矩阵索引：callee 是依赖
        }
        for (const auto &arg : expr.args)
        {
            if (arg)
                arg->accept(*this);
        }
    }

    void visit_index(const rel::IndexExpr &expr) override
    {
        if (expr.object)
            expr.object->accept(*this);
        for (const auto &idx : expr.indices)
        {
            if (idx)
                idx->accept(*this);
        }
    }

    void visit_grouping(const rel::GroupingExpr &expr) override
    {
        if (expr.inner)
            expr.inner->accept(*this);
    }

    void visit_sweep(const rel::SweepExpr &expr) override
    {
        for (const auto &item : expr.items)
        {
            if (item)
                item->accept(*this);
        }
    }

    void visit_matrix(const rel::MatrixExpr &expr) override
    {
        for (const auto &item : expr.items)
        {
            if (item)
                item->accept(*this);
        }
    }

    void visit_range(const rel::RangeExpr &expr) override
    {
        if (expr.start)
            expr.start->accept(*this);
        if (expr.step)
            expr.step->accept(*this);
        if (expr.stop)
            expr.stop->accept(*this);
    }

  private:
    void CollectReference(const rel::ReferenceExpr &expr)
    {
        if (expr.segments.empty())
            return;

        // 单段：注册函数 / 内置常量不是依赖
        if (expr.segments.size() == 1)
        {
            const std::string &name = expr.segments[0].name;
            if (rel::Environment::HasFunction(name))
                return;
            if (rel::Environment::FindConstant(name) != nullptr)
                return;
            out_.push_back(name);
            return;
        }

        // 多段路径：收集所有前缀（a.b.c -> a、a.b、a.b.c），
        // 与 Python 引擎的 visit_Attribute 前缀收集一致。
        std::string path;
        for (std::size_t i = 0; i < expr.segments.size(); ++i)
        {
            if (i > 0)
            {
                path += (expr.segments[i].sep == rel::RefSeparator::DDot) ? ".." : ".";
            }
            path += expr.segments[i].name;
            out_.push_back(path);
        }
    }

    std::vector<std::string> &out_;
};

// 去重保序（与 Python 引擎 list(dict.fromkeys(...)) 一致）
std::vector<std::string> Dedupe(const std::vector<std::string> &deps)
{
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto &d : deps)
    {
        if (seen.insert(d).second)
        {
            result.push_back(d);
        }
    }
    return result;
}

} // namespace

void RelParser::EvictLRU()
{
    // lru_cache 自管理容量，无需手动驱逐
}

std::vector<std::string> RelParser::ExtractDependencies(const rel::ExprPtr &expr) const
{
    std::vector<std::string> deps;
    if (expr)
    {
        RelDependencyVisitor visitor(deps);
        expr->accept(visitor);
    }
    return Dedupe(deps);
}

std::vector<std::string> RelParser::ExtractDependencies(const std::string &expression) const
{
    try
    {
        return ExtractDependencies(rel::Parse(expression));
    }
    catch (const std::exception &)
    {
        // 语法错误：不抛（错误由调用方通过 status/message 报告），依赖为空
        return {};
    }
}

std::size_t RelParser::FindBindingEq(const std::string &line)
{
    // 与 rel::Exec 一致：跳过比较运算符 ==, !=, <=, >=
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] != '=')
            continue;
        if (i > 0)
        {
            const char prev = line[i - 1];
            if (prev == '=' || prev == '!' || prev == '<' || prev == '>')
                continue;
        }
        if (i + 1 < line.size() && line[i + 1] == '=')
            continue;
        return i;
    }
    return std::string::npos;
}

std::string RelParser::Trim(const std::string &s)
{
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return s.substr(b, e - b);
}

bool RelParser::IsValidIdentifier(const std::string &name)
{
    auto is_ident_start = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    auto is_ident_char = [&](char c) { return is_ident_start(c) || (c >= '0' && c <= '9'); };

    if (name.empty() || !is_ident_start(name[0]))
        return false;
    for (std::size_t i = 1; i < name.size(); ++i)
    {
        if (!is_ident_char(name[i]))
            return false;
    }
    if (name == "if" || name == "then" || name == "elseif" || name == "else" ||
        name == "AND" || name == "OR" || name == "NOT" ||
        name == "EQUALS" || name == "NOTEQUALS")
        return false;
    return true;
}

std::vector<std::string> RelParser::SplitStatements(const std::string &code)
{
    std::vector<std::string> statements;

    std::string current;
    for (char c : code)
    {
        if (c == ';' || c == '\n')
        {
            if (!current.empty())
            {
                statements.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(c);
        }
    }
    if (!current.empty())
    {
        statements.push_back(current);
    }
    return statements;
}

ParseResult RelParser::ParseSingleStatement(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kStatement;

    ParseResultItem item;
    const std::size_t eq = FindBindingEq(code);

    if (eq != std::string::npos)
    {
        // 赋值语句：文本层拆分（同 rel::Exec），RHS 用 AST 提取依赖
        const std::string name = Trim(code.substr(0, eq));
        const std::string expr_str = Trim(code.substr(eq + 1));

        if (IsValidIdentifier(name))
        {
            item.name = name;
            item.content = expr_str;
            item.type = ItemType::kVariable;
            item.dependencies = ExtractDependencies(expr_str);
            item.status = ResultStatus::kSuccess;
        }
        else
        {
            item.name = "";
            item.content = code;
            item.type = ItemType::kError;
            item.message = "invalid identifier '" + name + "'";
            item.status = ResultStatus::kSyntaxError;
        }
    }
    else
    {
        // 非赋值表达式：rel::Parse 验证语法 + AST 提取依赖
        try
        {
            rel::ExprPtr expr = rel::Parse(code);
            item.name = "";
            item.content = code;
            item.type = ItemType::kUnknown;
            item.dependencies = ExtractDependencies(expr);
            item.status = ResultStatus::kSuccess;
        }
        catch (const std::exception &e)
        {
            item.name = "";
            item.content = code;
            item.type = ItemType::kError;
            item.message = e.what();
            item.status = ResultStatus::kSyntaxError;
        }
    }

    result.items.push_back(std::move(item));
    return result;
}

ParseResult RelParser::ParseStatements(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kStatement;

    const std::vector<std::string> statements = SplitStatements(code);
    for (const auto &stmt : statements)
    {
        ParseResult single = ParseSingleStatement(stmt);
        result.items.insert(result.items.end(), single.items.begin(), single.items.end());
    }
    return result;
}

ParseResult RelParser::ParseExpression(const std::string &code)
{
    ParseResult result;
    result.mode = ParseMode::kExpression;

    ParseResultItem item;
    item.name = "__expression__";
    item.content = code;
    item.type = ItemType::kUnknown;
    item.status = ResultStatus::kSuccess;

    try
    {
        rel::ExprPtr expr = rel::Parse(code);
        item.dependencies = ExtractDependencies(expr);
    }
    catch (const std::exception &e)
    {
        item.status = ResultStatus::kSyntaxError;
        item.message = e.what();
    }

    result.items.push_back(std::move(item));
    return result;
}

} // namespace rel_engine
} // namespace xequation
