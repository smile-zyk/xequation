#include "xequation_embed.h"

#include <stdexcept>

#include <Python.h>

XEquationProxy &XEquationProxy::GetInstance()
{
    // C++11 magic static：线程安全构造。构造函数只创建 REL 引擎，
    // 因此可在任意位置（含静态初始化阶段）安全调用。
    static XEquationProxy instance;
    return instance;
}

XEquationProxy::XEquationProxy()
{
    rel_manager_ = RelEquationEngine::GetInstance().CreateEquationManager();
}

XEquationProxy::~XEquationProxy()
{
    // Python 上下文持有 pybind11::dict，析构需要 GIL。
    // 解释器在进程退出前仍存活（无人调用 Py_FinalizeEx），此处安全。
    if (python_manager_)
    {
        pybind11::gil_scoped_acquire acquire;
        python_manager_.reset();
    }
    rel_manager_.reset();
}

EquationManager *XEquationProxy::GetManager(Engine engine)
{
    switch (engine)
    {
    case Engine::kPython:
        if (!python_manager_)
        {
            // 首次访问时初始化嵌入解释器（幂等）。未初始化且宿主未提供
            // 自定义配置时，注入构建期默认配置（REL_PYTHON_HOME 等）。
            if (!python_manager::PyEnvManager::IsInitialized())
            {
                python_manager::PyEnvManager::SetDefaultPyEnvConfig();
            }
            python_manager_ = PythonEquationEngine::GetInstance().CreateEquationManager();
        }
        return python_manager_.get();
    case Engine::kRel:
        return rel_manager_.get();
    }
    throw std::invalid_argument("XEquationProxy: unknown engine");
}

EquationManager &XEquationProxy::manager(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return *GetManager(engine);
}

// ---- 具体 Engine / Context 访问 ---------------------------------------------

PythonEquationEngine &XEquationProxy::python_engine()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(Engine::kPython); // 触发惰性创建
    return PythonEquationEngine::GetInstance();
}

RelEquationEngine &XEquationProxy::rel_engine()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(Engine::kRel); // 触发惰性创建
    return RelEquationEngine::GetInstance();
}

PythonEquationContext &XEquationProxy::python_context()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    xequation::EquationContext &ctx = GetManager(Engine::kPython)->context();
    return dynamic_cast<PythonEquationContext &>(ctx);
}

RelEquationContext &XEquationProxy::rel_context()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    xequation::EquationContext &ctx = GetManager(Engine::kRel)->context();
    return dynamic_cast<RelEquationContext &>(ctx);
}

// ---- 事件通知：原样透传 core 信号 --------------------------------------------

Connection XEquationProxy::ConnectEquationAdded(Engine engine, EquationAddedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationAdded>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationRemoving(Engine engine, EquationRemovingCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationRemoving>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationRemoved(Engine engine, EquationRemovedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationRemoved>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationUpdated(Engine engine, EquationUpdatedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationUpdated>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupAdded(Engine engine, EquationGroupAddedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupAdded>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupRemoving(Engine engine, EquationGroupRemovingCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupRemoving>(std::move(callback));
}

Connection XEquationProxy::ConnectEquationGroupUpdated(Engine engine, EquationGroupUpdatedCallback callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->signals_manager().Connect<xequation::EquationEvent::kEquationGroupUpdated>(std::move(callback));
}

void XEquationProxy::DisconnectAll(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->signals_manager().DisconnectAllEvent();
}

// ---- 编辑 ----------------------------------------------------------------------

EquationGroupId XEquationProxy::AddEquationGroup(Engine engine, const std::string &equation_statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->AddEquationGroup(equation_statement);
}

EquationGroupId XEquationProxy::AddEquation(Engine engine, const std::string &equation_name, const std::string &equation_content)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->AddEquation(equation_name, equation_content);
}

void XEquationProxy::EditEquationGroup(Engine engine, const EquationGroupId &group_id, const std::string &equation_statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->EditEquationGroup(group_id, equation_statement);
}

void XEquationProxy::EditSingleEquation(Engine engine, const EquationGroupId &group_id, const std::string &equation_name, const std::string &equation_content)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->EditSingleEquation(group_id, equation_name, equation_content);
}

void XEquationProxy::RemoveEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->RemoveEquationGroup(group_id);
}

// ---- 解析 / 计算 ----------------------------------------------------------------

ParseResult XEquationProxy::Parse(Engine engine, const std::string &expression, ParseMode mode)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Parse(expression, mode);
}

InterpretResult XEquationProxy::Eval(Engine engine, const std::string &expression)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Eval(expression);
}

InterpretResult XEquationProxy::Exec(Engine engine, const std::string &statement)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->Exec(statement);
}

void XEquationProxy::Update(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->Update();
}

void XEquationProxy::UpdateEquation(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->UpdateEquation(equation_name);
}

void XEquationProxy::UpdateEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->UpdateEquationGroup(group_id);
}

// ---- 变量读写 / 查询 --------------------------------------------------------------

void XEquationProxy::SetValue(Engine engine, const std::string &name, const EquationValue &value)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GetManager(engine)->context().Set(name, value);
}

EquationValue XEquationProxy::GetValue(Engine engine, const std::string &name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->context().Get(name);
}

std::vector<std::string> XEquationProxy::GetEquationNames(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquationNames();
}

std::vector<std::string> XEquationProxy::GetContextKeys(Engine engine)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto keys = GetManager(engine)->context().keys();
    return std::vector<std::string>(keys.begin(), keys.end());
}

bool XEquationProxy::IsEquationExist(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->IsEquationExist(equation_name);
}

const Equation *XEquationProxy::GetEquation(Engine engine, const std::string &equation_name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquation(equation_name);
}

const EquationGroup *XEquationProxy::GetEquationGroup(Engine engine, const EquationGroupId &group_id)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return GetManager(engine)->GetEquationGroup(group_id);
}
