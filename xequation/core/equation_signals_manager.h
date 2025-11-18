#pragma once

#include <boost/signals2.hpp>

#include "bitmask.hpp"
#include "core/equation_group.h"
#include "equation.h"
#include "equation_group.h"

namespace xequation {

enum class EquationField
{
    kContent = 0x01,
    kType = 0x02,
    kStatus = 0x04,
    kMessage = 0x08,
    kDependencies = 0x10,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationField, kDependencies)

enum class EquationGroupField
{
    kStatement = 0x01,
    kEquations = 0x02,
};

BITMASK_DEFINE_MAX_ELEMENT(EquationGroupField, kEquations)

enum class EquationEvent
{
    kEquationAdded,
    kEquationRemoving,
    kEquationFieldUpdate,
    kEquationGroupAdded,
    kEquationGroupRemoving,
    kEquationGroupFieldUpdate,
};

using EquationAddedCallback = std::function<void(const Equation*)>;
using EquationRemovingCallback = std::function<void(const Equation*)>;
using EquationUpdateCallback = std::function<void(const Equation*, bitmask::bitmask<EquationField>)>;
using EquationGroupAddedCallback = std::function<void(const EquationGroup*)>;
using EquationGroupRemovingCallback = std::function<void(const EquationGroup*)>;
using EquationGroupUpdateCallback = std::function<void(const EquationGroup*, bitmask::bitmask<EquationGroupField>)>;

using Connection = boost::signals2::connection;
using ScopedConnection = boost::signals2::scoped_connection;

class EquationSignalsManager
{
private:
    using EquationAddedSignal = boost::signals2::signal<void(const Equation*)>;
    using EquationRemovingSignal = boost::signals2::signal<void(const Equation*)>;
    using EquationUpdateSignal = boost::signals2::signal<void(const Equation*, bitmask::bitmask<EquationField>)>;
    using EquationGroupAddedSignal = boost::signals2::signal<void(const EquationGroup*)>;
    using EquationGroupRemovingSignal = boost::signals2::signal<void(const EquationGroup*)>;
    using EquationGroupUpdateSignal = boost::signals2::signal<void(const EquationGroup*, bitmask::bitmask<EquationGroupField>)>;

    std::unordered_map<EquationEvent, std::unique_ptr<boost::signals2::signal_base>> signals_;
    
    template<EquationEvent Event> struct GetSignalType;
    
    template<> struct GetSignalType<EquationEvent::kEquationAdded> 
    { using type = EquationAddedSignal; };
    
    template<> struct GetSignalType<EquationEvent::kEquationRemoving> 
    { using type = EquationRemovingSignal; };
    
    template<> struct GetSignalType<EquationEvent::kEquationFieldUpdate> 
    { using type = EquationUpdateSignal; };
    
    template<> struct GetSignalType<EquationEvent::kEquationGroupAdded> 
    { using type = EquationGroupAddedSignal; };
    
    template<> struct GetSignalType<EquationEvent::kEquationGroupRemoving> 
    { using type = EquationGroupRemovingSignal; };
    
    template<> struct GetSignalType<EquationEvent::kEquationGroupFieldUpdate> 
    { using type = EquationGroupUpdateSignal; };

    template<EquationEvent Event> struct GetCallbackType;
    
    template<> struct GetCallbackType<EquationEvent::kEquationAdded> 
    { using type = EquationAddedCallback; };
    
    template<> struct GetCallbackType<EquationEvent::kEquationRemoving> 
    { using type = EquationRemovingCallback; };
    
    template<> struct GetCallbackType<EquationEvent::kEquationFieldUpdate> 
    { using type = EquationUpdateCallback; };
    
    template<> struct GetCallbackType<EquationEvent::kEquationGroupAdded> 
    { using type = EquationGroupAddedCallback; };
    
    template<> struct GetCallbackType<EquationEvent::kEquationGroupRemoving> 
    { using type = EquationGroupRemovingCallback; };
    
    template<> struct GetCallbackType<EquationEvent::kEquationGroupFieldUpdate> 
    { using type = EquationGroupUpdateCallback; };

public:
    EquationSignalsManager()
    {
        signals_[EquationEvent::kEquationAdded] = std::make_unique<EquationAddedSignal>();
        signals_[EquationEvent::kEquationRemoving] = std::make_unique<EquationRemovingSignal>();
        signals_[EquationEvent::kEquationFieldUpdate] = std::make_unique<EquationUpdateSignal>();
        signals_[EquationEvent::kEquationGroupAdded] = std::make_unique<EquationGroupAddedSignal>();
        signals_[EquationEvent::kEquationGroupRemoving] = std::make_unique<EquationGroupRemovingSignal>();
        signals_[EquationEvent::kEquationGroupFieldUpdate] = std::make_unique<EquationGroupUpdateSignal>();
    }

    EquationSignalsManager(const EquationSignalsManager&) = delete;
    EquationSignalsManager& operator=(const EquationSignalsManager&) = delete;
    
    EquationSignalsManager(EquationSignalsManager&&) = delete;
    EquationSignalsManager& operator=(EquationSignalsManager&&) = delete;

    template<EquationEvent Event>
    Connection connect(typename GetCallbackType<Event>::type callback)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto* signal = static_cast<SignalType*>(it->second.get());
        return signal->connect(callback);
    }

    template<EquationEvent Event>
    ScopedConnection connect_scoped(typename GetCallbackType<Event>::type callback)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto* signal = static_cast<SignalType*>(it->second.get());
        return ScopedConnection(signal->connect(callback));
    }

    template<EquationEvent Event, typename... Args>
    void emit(Args&&... args)
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto* signal = static_cast<SignalType*>(it->second.get());
        (*signal)(std::forward<Args>(args)...);
    }

    void disconnect(Connection& connection)
    {
        connection.disconnect();
    }

    template<EquationEvent Event>
    void disconnect_all()
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        it->second-disconnect_all_slots();
    }

    template<EquationEvent Event>
    bool empty() const
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto* signal = static_cast<SignalType*>(it->second.get());
        return signal->empty();
    }

    template<EquationEvent Event>
    std::size_t num_slots() const
    {
        auto it = signals_.find(Event);
        if (it == signals_.end()) {
            throw std::invalid_argument("Unknown event type");
        }

        using SignalType = typename GetSignalType<Event>::type;
        auto* signal = static_cast<SignalType*>(it->second.get());
        return signal->num_slots();
    }
};

}