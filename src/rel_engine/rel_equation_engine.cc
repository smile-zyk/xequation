#include "rel_equation_engine.h"

#include <memory>

#include "rel_equation_context.h"

namespace xequation
{
namespace rel_engine
{

RelEquationEngine::RelEquationEngine()
{
    engine_info_.name = "REL";

    // 进程启动时初始化 REL 的内置常量（PI/e/c0...）和函数库（builtin/math）。
    // 幂等：重复调用无副作用。
    rel::Environment::InitBuiltinConstants();
    rel::Environment::InitBuiltinFunctions();

    code_parser = std::unique_ptr<RelParser>(new RelParser());
    code_executor = std::unique_ptr<RelExecutor>(new RelExecutor());
}

RelEquationEngine::~RelEquationEngine() = default;

namespace
{
// 取出 REL 求值目标环境：context 必须是 RelEquationContext（或 nullptr 用临时环境）
rel::Environment *ResolveRelEnv(const EquationContext *context, rel::Environment &fallback)
{
    const RelEquationContext *rel_context = dynamic_cast<const RelEquationContext *>(context);
    if (rel_context)
    {
        // env() 返回非 const 引用；context 是 const 指针，这里显式转换以复用
        // 方程管理器持有的上下文环境（变量跨方程可见）。
        return const_cast<rel::Environment *>(&rel_context->env());
    }
    return &fallback;
}
} // namespace

InterpretResult RelEquationEngine::Eval(const std::string &code, const EquationContext *context)
{
    rel::Environment fallback_env;
    rel::Environment *target = ResolveRelEnv(context, fallback_env);
    return code_executor->Eval(code, *target);
}

InterpretResult RelEquationEngine::Exec(const std::string &code, const EquationContext *context)
{
    rel::Environment fallback_env;
    rel::Environment *target = ResolveRelEnv(context, fallback_env);
    return code_executor->Exec(code, *target);
}

ParseResult RelEquationEngine::Parse(const std::string &code, ParseMode mode)
{
    if (mode == ParseMode::kExpression)
    {
        return code_parser->ParseExpression(code);
    }
    else
    {
        return code_parser->ParseStatements(code);
    }
}

std::unique_ptr<EquationContext> RelEquationEngine::CreateContext()
{
    return std::unique_ptr<EquationContext>(new RelEquationContext(engine_info_));
}

} // namespace rel_engine
} // namespace xequation
