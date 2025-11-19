#pragma once

#include <boost/signals2.hpp>
#include <memory>

#include "bitmask.hpp"
#include "core/equation_group.h"
#include "equation.h"
#include "equation_group.h"

namespace xequation
{

enum class EquationField
{
    kContent = 1 << 0,
    kType = 1 << 1,
    kStatus = 1 << 2,
    kMessage = 1 << 3,
    kDependencies = 1 << 4,
    kValue = 1 << 5,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationField, kDependencies)

enum class EquationGroupField
{
    kStatement = 1 << 0,
    kEquationCount = 1 << 1,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationGroupField, kEquationCount)

enum class EquationEvent
{
    kEquationAdded,
    kEquationRemoving,
    kEquationUpdate,
    kEquationGroupAdded,
    kEquationGroupRemoving,
    kEquationGroupUpdate,
};

class EquationSignalsManager
{
  private:
    using EquationAddedCallback = std::function<void(const Equation *)>;
    using EquationRemovingCallback = std::function<void(const Equation *)>;
    using EquationUpdateCallback = std::function<void(const Equation *, bitmask::bitmask<EquationField>)>;
    using EquationGroupAddedCallback = std::function<void(const EquationGroup *)>;
    using EquationGroupRemovingCallback = std::function<void(const EquationGroup *)>;
    using EquationGroupUpdateCallback =
        std::function<void(const EquationGroup *, bitmask::bitmask<EquationGroupField>)>;

    using Connection = boost::signals2::connection;
    using ScopedConnection = boost::signals2::scoped_connection;

    using EquationAddedSignal = boost::signals2::signal<void(const Equation *)>;
    using EquationRemovingSignal = boost::signals2::signal<void(const Equation *)>;
    using EquationUpdateSignal = boost::signals2::signal<void(const Equation *, bitmask::bitmask<EquationField>)>;
    using EquationGroupAddedSignal = boost::signals2::signal<void(const EquationGroup *)>;
    using EquationGroupRemovingSignal = boost::signals2::signal<void(const EquationGroup *)>;
    using EquationGroupUpdateSignal =
        boost::signals2::signal<void(const EquationGroup *, bitmask::bitmask<EquationGroupField>)>;

    std::unordered_map<EquationEvent, std::unique_ptr<boost::signals2::signal_base>> signals_;

    template <EquationEvent Event>
    struct GetSignalType;

    template <EquationEvent Event>
    struct GetCallbackType;

  public:
    EquationSignalsManager()
    {
        EquationAddedSignal signal;
        signals_[EquationEvent::kEquationAdded] = std::unique_ptr<EquationAddedSignal>(new EquationAddedSignal());
        signals_[EquationEvent::kEquationRemoving] =
            std::unique_ptr<EquationRemovingSignal>(new EquationRemovingSignal());
        signals_[EquationEvent::kEquationUpdate] = std::unique_ptr<EquationUpdateSignal>(new EquationUpdateSignal());
        signals_[EquationEvent::kEquationGroupAdded] =
            std::unique_ptr<EquationGroupAddedSignal>(new EquationGroupAddedSignal());
        signals_[EquationEvent::kEquationGroupRemoving] =
            std::unique_ptr<EquationGroupRemovingSignal>(new EquationGroupRemovingSignal());
        signals_[EquationEvent::kEquationGroupUpdate] =
            std::unique_ptr<EquationGroupUpdateSignal>(new EquationGroupUpdateSignal());
    }

    EquationSignalsManager(const EquationSignalsManager &) = delete;
    EquationSignalsManager &operator=(const EquationSignalsManager &) = delete;

    EquationSignalsManager(EquationSignalsManager &&) = delete;
    EquationSignalsManager &operator=(EquationSignalsManager &&) = delete;

    template <EquationEvent Event>
    Connection connect(typename GetCallbackType<Event>::type callback)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        return signal->connect(callback);
    }

    template <EquationEvent Event>
    ScopedConnection connect_scoped(typename GetCallbackType<Event>::type callback)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        return ScopedConnection(signal->connect(callback));
    }

    template <EquationEvent Event, typename... Args>
    void emit(Args &&...args)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        (*signal)(std::forward<Args>(args)...);
    }

    void disconnect(Connection &connection)
    {
        connection.disconnect();
    }

    template <EquationEvent Event>
    void disconnect_all()
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        return signal->disconnect_all_slots();
    }

    void disconnect_all_event()
    {
        disconnect_all<EquationEvent::kEquationAdded>();
        disconnect_all<EquationEvent::kEquationRemoving>();
        disconnect_all<EquationEvent::kEquationUpdate>();
        disconnect_all<EquationEvent::kEquationGroupAdded>();
        disconnect_all<EquationEvent::kEquationGroupRemoving>();
        disconnect_all<EquationEvent::kEquationGroupUpdate>();
    }

    template <EquationEvent Event>
    bool empty() const
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        return signal->empty();
    }

    template <EquationEvent Event>
    std::size_t num_slots() const
    {
        auto it = signals_.find(Event);
        if (it == signals_.end())
        {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto *signal = static_cast<SignalType *>(it->second.get());
        return signal->num_slots();
    }
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationAdded>
{
    using type = EquationAddedSignal;
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationRemoving>
{
    using type = EquationRemovingSignal;
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationUpdate>
{
    using type = EquationUpdateSignal;
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationGroupAdded>
{
    using type = EquationGroupAddedSignal;
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationGroupRemoving>
{
    using type = EquationGroupRemovingSignal;
};

template <>
struct EquationSignalsManager::GetSignalType<EquationEvent::kEquationGroupUpdate>
{
    using type = EquationGroupUpdateSignal;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationAdded>
{
    using type = EquationAddedCallback;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationRemoving>
{
    using type = EquationRemovingCallback;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationUpdate>
{
    using type = EquationUpdateCallback;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationGroupAdded>
{
    using type = EquationGroupAddedCallback;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationGroupRemoving>
{
    using type = EquationGroupRemovingCallback;
};

template <>
struct EquationSignalsManager::GetCallbackType<EquationEvent::kEquationGroupUpdate>
{
    using type = EquationGroupUpdateCallback;
};
} // namespace xequation