#pragma once

#include <boost/signals2.hpp>
#include <memory>

#include "bitmask.hpp"
#include "core/equation_group.h"
#include "equation.h"
#include "equation_group.h"

namespace xequation
{

enum class EquationUpdateFlag
{
    kContent = 1 << 0,
    kType = 1 << 1,
    kStatus = 1 << 2,
    kMessage = 1 << 3,
    kValue = 1 << 5,
    kDependencies = 1 << 6,
    kDependents = 1 << 7,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationUpdateFlag, kDependents)

enum class EquationGroupUpdateFlag
{
    kStatement = 1 << 0,
    kEquationCount = 1 << 1,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationGroupUpdateFlag, kEquationCount)

enum class EquationEvent
{
    kEquationAdded,
    kEquationRemoving,
    kEquationUpdated,
    kEquationGroupAdded,
    kEquationGroupRemoving,
    kEquationGroupUpdated,
};

using EquationAddedCallback = std::function<void(const Equation *)>;
using EquationRemovingCallback = std::function<void(const Equation *)>;
using EquationUpdatedCallback = std::function<void(const Equation *, bitmask::bitmask<EquationUpdateFlag>)>;
using EquationGroupAddedCallback = std::function<void(const EquationGroup *)>;
using EquationGroupRemovingCallback = std::function<void(const EquationGroup *)>;
using EquationGroupUpdatedCallback = std::function<void(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>)>;

using Connection = boost::signals2::connection;
using ScopedConnection = boost::signals2::scoped_connection;

using EquationAddedSignal = boost::signals2::signal<void(const Equation *)>;
using EquationRemovingSignal = boost::signals2::signal<void(const Equation *)>;
using EquationUpdatedSignal = boost::signals2::signal<void(const Equation *, bitmask::bitmask<EquationUpdateFlag>)>;
using EquationGroupAddedSignal = boost::signals2::signal<void(const EquationGroup *)>;
using EquationGroupRemovingSignal = boost::signals2::signal<void(const EquationGroup *)>;
using EquationGroupUpdatedSignal =
    boost::signals2::signal<void(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>)>;

template <EquationEvent Event>
struct GetSignalType;

template <EquationEvent Event>
struct GetCallbackType;

template <>
struct GetSignalType<EquationEvent::kEquationAdded>
{
    using type = EquationAddedSignal;
};

template <>
struct GetSignalType<EquationEvent::kEquationRemoving>
{
    using type = EquationRemovingSignal;
};

template <>
struct GetSignalType<EquationEvent::kEquationUpdated>
{
    using type = EquationUpdatedSignal;
};

template <>
struct GetSignalType<EquationEvent::kEquationGroupAdded>
{
    using type = EquationGroupAddedSignal;
};

template <>
struct GetSignalType<EquationEvent::kEquationGroupRemoving>
{
    using type = EquationGroupRemovingSignal;
};

template <>
struct GetSignalType<EquationEvent::kEquationGroupUpdated>
{
    using type = EquationGroupUpdatedSignal;
};

template <>
struct GetCallbackType<EquationEvent::kEquationAdded>
{
    using type = EquationAddedCallback;
};

template <>
struct GetCallbackType<EquationEvent::kEquationRemoving>
{
    using type = EquationRemovingCallback;
};

template <>
struct GetCallbackType<EquationEvent::kEquationUpdated>
{
    using type = EquationUpdatedCallback;
};

template <>
struct GetCallbackType<EquationEvent::kEquationGroupAdded>
{
    using type = EquationGroupAddedCallback;
};

template <>
struct GetCallbackType<EquationEvent::kEquationGroupRemoving>
{
    using type = EquationGroupRemovingCallback;
};

template <>
struct GetCallbackType<EquationEvent::kEquationGroupUpdated>
{
    using type = EquationGroupUpdatedCallback;
};

class EquationSignalsManager
{
  private:
    std::unordered_map<EquationEvent, std::unique_ptr<boost::signals2::signal_base>> signals_;

  public:
    EquationSignalsManager()
    {
        signals_[EquationEvent::kEquationAdded] = std::unique_ptr<EquationAddedSignal>(new EquationAddedSignal());
        signals_[EquationEvent::kEquationRemoving] =
            std::unique_ptr<EquationRemovingSignal>(new EquationRemovingSignal());
        signals_[EquationEvent::kEquationUpdated] = std::unique_ptr<EquationUpdatedSignal>(new EquationUpdatedSignal());
        signals_[EquationEvent::kEquationGroupAdded] =
            std::unique_ptr<EquationGroupAddedSignal>(new EquationGroupAddedSignal());
        signals_[EquationEvent::kEquationGroupRemoving] =
            std::unique_ptr<EquationGroupRemovingSignal>(new EquationGroupRemovingSignal());
        signals_[EquationEvent::kEquationGroupUpdated] =
            std::unique_ptr<EquationGroupUpdatedSignal>(new EquationGroupUpdatedSignal());
    }

    EquationSignalsManager(const EquationSignalsManager &) = delete;
    EquationSignalsManager &operator=(const EquationSignalsManager &) = delete;

    EquationSignalsManager(EquationSignalsManager &&) = delete;
    EquationSignalsManager &operator=(EquationSignalsManager &&) = delete;

    template <EquationEvent Event>
    Connection Connect(typename GetCallbackType<Event>::type callback) const
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
    ScopedConnection ConnectScoped(typename GetCallbackType<Event>::type callback) const
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
    void Emit(Args &&...args) const
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

    void Disconnect(Connection &connection) const
    {
        connection.disconnect();
    }

    template <EquationEvent Event>
    void DisconnectAll() const
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

    void DisconnectAllEvent() const
    {
        DisconnectAll<EquationEvent::kEquationAdded>();
        DisconnectAll<EquationEvent::kEquationRemoving>();
        DisconnectAll<EquationEvent::kEquationUpdated>();
        DisconnectAll<EquationEvent::kEquationGroupAdded>();
        DisconnectAll<EquationEvent::kEquationGroupRemoving>();
        DisconnectAll<EquationEvent::kEquationGroupUpdated>();
    }

    template <EquationEvent Event>
    bool IsEmpty() const
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
    std::size_t GetNumSlots() const
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

} // namespace xequation