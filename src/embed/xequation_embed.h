#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/equation_manager.h"
#include "core/equation_signals_manager.h"
#include "python/python_equation_context.h"
#include "python/python_equation_engine.h"
#include "rel_engine/rel_equation_context.h"
#include "rel_engine/rel_equation_engine.h"

// 将 API 涉及的 xequation 类型提升到全局命名空间，
// 宿主调用时无需任何命名空间前缀。
using xequation::Connection;
using xequation::Equation;
using xequation::EquationAddedCallback;
using xequation::EquationGroup;
using xequation::EquationGroupAddedCallback;
using xequation::EquationGroupId;
using xequation::EquationGroupRemovingCallback;
using xequation::EquationGroupUpdateFlag;
using xequation::EquationGroupUpdatedCallback;
using xequation::EquationManager;
using xequation::EquationRemovedCallback;
using xequation::EquationRemovingCallback;
using xequation::EquationUpdateFlag;
using xequation::EquationUpdatedCallback;
using xequation::EquationValue;
using xequation::InterpretResult;
using xequation::ParseMode;
using xequation::ParseResult;
using xequation::ResultStatus;
using xequation::python::PythonEquationContext;
using xequation::python::PythonEquationEngine;
using xequation::rel_engine::RelEquationContext;
using xequation::rel_engine::RelEquationEngine;

enum class Engine
{
    kPython,
    kRel,
};

// 进程级单例门面：内部持有一个 Python EquationManager 和一个 REL
// EquationManager，供宿主计算/编辑/解析表达式。
//
//  - 任意位置通过 GetInstance() 访问；
//  - 事件通知【原样透传】core 信号，回调签名见 equation_signals_manager.h，
//    连接管理（Connect*/DisconnectAll）收敛在本类；
//  - 内部用递归锁保护，回调内重入安全；
//  - Python 引擎首次访问时才初始化嵌入解释器（幂等）。
class XEquationProxy
{
  public:
    static XEquationProxy &GetInstance();

    // 通用 Manager 访问
    EquationManager &manager(Engine engine);

    // ---- 具体 Engine / Context 访问 ----
    // Engine 为进程级单例；Context 为对应 Manager 持有的具体上下文
    // （Python: pybind11::dict 包装；REL: rel::Environment 包装），
    // 首次访问会惰性创建对应引擎。
    PythonEquationEngine &python_engine();
    RelEquationEngine &rel_engine();

    PythonEquationContext &python_context();
    RelEquationContext &rel_context();

    // ---- 事件通知：原样透传 core 信号 ----

    Connection ConnectEquationAdded(Engine engine, EquationAddedCallback callback);
    Connection ConnectEquationRemoving(Engine engine, EquationRemovingCallback callback);
    Connection ConnectEquationRemoved(Engine engine, EquationRemovedCallback callback);
    Connection ConnectEquationUpdated(Engine engine, EquationUpdatedCallback callback);
    Connection ConnectEquationGroupAdded(Engine engine, EquationGroupAddedCallback callback);
    Connection ConnectEquationGroupRemoving(Engine engine, EquationGroupRemovingCallback callback);
    Connection ConnectEquationGroupUpdated(Engine engine, EquationGroupUpdatedCallback callback);
    void DisconnectAll(Engine engine);

    // ---- 编辑 ----

    EquationGroupId AddEquationGroup(Engine engine, const std::string &equation_statement);
    EquationGroupId AddEquation(Engine engine, const std::string &equation_name, const std::string &equation_content);
    void EditEquationGroup(Engine engine, const EquationGroupId &group_id, const std::string &equation_statement);
    void EditSingleEquation(Engine engine, const EquationGroupId &group_id, const std::string &equation_name, const std::string &equation_content);
    void RemoveEquationGroup(Engine engine, const EquationGroupId &group_id);

    // ---- 解析 / 计算 ----

    ParseResult Parse(Engine engine, const std::string &expression, ParseMode mode);
    InterpretResult Eval(Engine engine, const std::string &expression);
    InterpretResult Exec(Engine engine, const std::string &statement);

    void Update(Engine engine);
    void UpdateEquation(Engine engine, const std::string &equation_name);
    void UpdateEquationGroup(Engine engine, const EquationGroupId &group_id);

    // ---- 变量读写 / 查询 ----

    void SetValue(Engine engine, const std::string &name, const EquationValue &value);
    EquationValue GetValue(Engine engine, const std::string &name);
    std::vector<std::string> GetEquationNames(Engine engine);
    std::vector<std::string> GetContextKeys(Engine engine);
    bool IsEquationExist(Engine engine, const std::string &equation_name);

    // 按名称获取 Equation，可读取 name/content/status/message/GetValue()，
    // 用于在宿主里复刻 rel_demo 的 ShowEquation 展示逻辑。
    const Equation *GetEquation(Engine engine, const std::string &equation_name);
    const EquationGroup *GetEquationGroup(Engine engine, const EquationGroupId &group_id);

  private:
    XEquationProxy();
    ~XEquationProxy();
    XEquationProxy(const XEquationProxy &) = delete;
    XEquationProxy &operator=(const XEquationProxy &) = delete;

    EquationManager *GetManager(Engine engine);

    std::unique_ptr<EquationManager> python_manager_;
    std::unique_ptr<EquationManager> rel_manager_;
    std::recursive_mutex mutex_;
};
