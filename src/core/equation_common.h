#pragma once

#include <exception>
#include <ostream>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include "bitmask.hpp"
#include "equation_value.h"

namespace xequation
{
// 结果状态：粗粒度。具体错误细节（如“变量 x 未定义”/“除以零”）统一放在
// InterpretResult::message / Equation::message 中，这里不再细分错误类型。
enum class ResultStatus
{
    kPending,
    kCalculating,
    kSuccess,
    kError
};

struct InterpretResult
{
    ResultStatus status;
    std::string message;
    EquationValue value;
};

// =========================================================================
//  Registered expression (只算不存)
//
//  A registered expression is an "observe-only" computation:
//    - It is parsed for dependencies and participates in the dependency graph,
//      so it is recomputed whenever the equations it depends on change.
//    - It is NEVER written into the environment.  Its result is stored in
//      this object (via EquationManager::GetExpressionValue) and can only be
//      consumed by the host.
//    - It has no Equation, so it does not show up in GetEquationNames().
// =========================================================================
using ObjectId = boost::uuids::uuid;

struct Expression
{
    ObjectId id;
    std::string content;
    // Full result of the last evaluation (Eval).  status/message/value
    // are accessed via result.status / result.message / result.value.
    InterpretResult result;
    std::vector<std::string> dependencies;
};

// =========================================================================
//  Parse 结果（单个表达式）
//
//  引擎只负责“解析一个表达式”：做语法校验并提取它引用的依赖符号。
//  名字绑定（Equation 的 name）由 EquationManager 自己完成，不在 Parse 里。
// =========================================================================
struct ParseResult
{
    ResultStatus status = ResultStatus::kSuccess;  // 语法校验结果
    std::string message;                            // 语法错误详情
    std::vector<std::string> dependencies;          // 该表达式引用的符号
};

class ParseException : public std::exception
{
  private:
    std::string error_message_;

  public:
    ParseException(const std::string &message) : error_message_(message) {}

    const char *what() const noexcept override
    {
        return error_message_.c_str();
    }

    const std::string &error_message() const
    {
        return error_message_;
    }
};

enum class EquationUpdateFlag
{
    kName = 1 << 0,
    kContent = 1 << 1,
    kStatus = 1 << 2,
    kMessage = 1 << 3,
    kValue = 1 << 4,
    kDependencies = 1 << 5,
    kDependents = 1 << 6,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationUpdateFlag, kDependents)

enum class ExpressionUpdateFlag
{
    kContent = 1 << 0,
    kStatus = 1 << 1,
    kMessage = 1 << 2,
    kValue = 1 << 3,
    kDependencies = 1 << 4,
};

BITMASK_DEFINE_MAX_ELEMENT(ExpressionUpdateFlag, kDependencies)

class ResultStatusConverter
{
  public:
    static ResultStatus FromString(const std::string &status_str)
    {
        if (status_str == "Pending")
            return ResultStatus::kPending;
        else if (status_str == "Calculating")
            return ResultStatus::kCalculating;
        else if (status_str == "Success")
            return ResultStatus::kSuccess;
        else
            return ResultStatus::kError;
    }

    static std::string ToString(ResultStatus status)
    {
        switch (status)
        {
        case ResultStatus::kPending:
            return "Pending";
        case ResultStatus::kCalculating:
            return "Calculating";
        case ResultStatus::kSuccess:
            return "Success";
        default:
            return "Error";
        }
    }
};

inline std::ostream &operator<<(std::ostream &os, ResultStatus status)
{
    return os << ResultStatusConverter::ToString(status);
}
} // namespace xequation
