#include "rel_equation_context.h"

namespace xequation
{
namespace rel_engine
{

RelEquationContext::RelEquationContext(const EquationEngineInfo &engine_info)
{
    engine_info_ = engine_info;
}

bool RelEquationContext::Contains(const std::string &key) const
{
    if (removed_.count(key))
    {
        return false;
    }
    const std::vector<std::string> names = env_.VariableNames();
    return std::find(names.begin(), names.end(), key) != names.end();
}

EquationValue RelEquationContext::Get(const std::string &key) const
{
    if (!Contains(key))
    {
        return EquationValue::Null();
    }
    return EquationValue(env_.Get(key));
}

void RelEquationContext::Set(const std::string &key, const EquationValue &value)
{
    if (value.IsRelValue())
    {
        env_.Define(key, value.AsRel());
        removed_.erase(key);
        return;
    }
    // PyObjectRef 等非 rel::Value 载荷无法直接写入 rel::Environment
    throw std::runtime_error(
        "RelEquationContext::Set: only rel::Value payloads can be stored in the REL environment"
    );
}

bool RelEquationContext::Remove(const std::string &key)
{
    if (!Contains(key))
    {
        return false;
    }
    removed_.insert(key);
    return true;
}

void RelEquationContext::Clear()
{
    removed_.clear();
    // rel::Environment 没有清空 API；用 VariableNames 逐个标记移除，
    // 保留底层的绑定（重新赋值会覆盖）。
    for (const auto &name : env_.VariableNames())
    {
        removed_.insert(name);
    }
}

size_t RelEquationContext::size() const
{
    return keys().size();
}

bool RelEquationContext::empty() const
{
    return keys().empty();
}

std::unordered_set<std::string> RelEquationContext::keys() const
{
    std::unordered_set<std::string> key_set;
    for (const auto &name : env_.VariableNames())
    {
        if (!removed_.count(name))
        {
            key_set.insert(name);
        }
    }
    return key_set;
}

std::vector<std::string> RelEquationContext::GetBuiltinNames() const
{
    std::vector<std::string> names;

    // 内置常量（PI、e、c0 ...）
    const std::vector<std::string> constants = rel::Environment::ConstantNames();
    names.insert(names.end(), constants.begin(), constants.end());

    // 注册函数（builtin + math 库）
    const std::vector<std::string> functions = rel::Environment::FunctionNames();
    names.insert(names.end(), functions.begin(), functions.end());

    return names;
}

std::vector<std::string> RelEquationContext::GetSymbolNames() const
{
    std::vector<std::string> names;
    const std::vector<std::string> vars = env_.VariableNames();
    for (const auto &name : vars)
    {
        if (!removed_.count(name))
        {
            names.push_back(name);
        }
    }
    return names;
}

std::string RelEquationContext::GetSymbolType(const std::string &symbol_name) const
{
    // 函数优先（全局注册表）
    if (rel::Environment::HasFunction(symbol_name))
    {
        return "function";
    }
    // 内置常量
    if (rel::Environment::FindConstant(symbol_name) != nullptr)
    {
        return "constant";
    }
    // 用户变量：按 rel::Value 的 data_type 归类
    if (Contains(symbol_name))
    {
        const rel::Value &v = env_.Get(symbol_name);
        switch (v.data_type())
        {
            case xdataset::DataType::kReal:    return "real";
            case xdataset::DataType::kInteger: return "integer";
            case xdataset::DataType::kBoolean: return "boolean";
            case xdataset::DataType::kString:  return "string";
            case xdataset::DataType::kComplex: return "complex";
        }
    }
    return "";
}

std::string RelEquationContext::GetTypeCategory(const std::string &type_name) const
{
    // 归类为 Function / Variable（REL 无模块/类）
    if (type_name == "function" || type_name == "constant")
    {
        return "Function";
    }
    return "Variable";
}

} // namespace rel_engine
} // namespace xequation
