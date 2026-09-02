#pragma once

#include <boost/uuid/uuid.hpp>
#include <string>
#include <vector>

#include "equation_common.h"

namespace xequation
{

// =========================================================================
//  Equation —— 名字 -> 表达式的绑定（纯数据）
//
//  与 Expression 对齐的简单数据容器：
//    - id            唯一身份（uuid，不随重命名改变）
//    - name          绑定到环境中的变量名（也是依赖图节点名）
//    - content       表达式文本
//    - status        最近一次求值状态
//    - message       求值消息 / 错误详情
//    - dependencies  最近一次解析出的依赖符号（仅供展示；依赖图是权威）
//
//  不持有 manager 指针，没有 GetValue/GetDependencies 等方法：当前绑定
//  的值经 EquationManager::GetVariable(name) 从 env 读取（env 是唯一
//  权威）；依赖/依赖者经 EquationManager 查询依赖图。
// =========================================================================
struct Equation
{
    ObjectId id;
    std::string name;
    std::string content;
    ResultStatus status = ResultStatus::kPending;
    std::string message;
    std::vector<std::string> dependencies;
};

} // namespace xequation
