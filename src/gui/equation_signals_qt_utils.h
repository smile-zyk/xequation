#pragma once

#include <type_traits>
#include <QMetaObject>
#include <QMetaMethod>
#include <QObject>
#include <core/equation_signals_manager.h>

namespace xequation
{
namespace gui
{

template<EquationEvent Event, typename T, Qt::ConnectionType Connection = Qt::QueuedConnection>
struct EquationQtSignalTraits;

// Direct connection specialization for kEquationAdded
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationAdded, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            (receiver->*slot)(equation);
        };
    }
};

// Queued connection specialization for kEquationAdded
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationAdded, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            QMetaObject::invokeMethod(
                const_cast<T*>(receiver),
                [receiver, slot, equation]() { (receiver->*slot)(equation); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationAdded (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationAdded, T, Connection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            QMetaObject::invokeMethod(
                const_cast<T*>(receiver),
                [receiver, slot, equation]() { (receiver->*slot)(equation); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationRemoving
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoving, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            (receiver->*slot)(equation);
        };
    }
};

// Queued connection specialization for kEquationRemoving
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoving, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation]() { (receiver->*slot)(equation); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationRemoving (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoving, T, Connection>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation]() { (receiver->*slot)(equation); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationRemoved
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoved, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const std::string &);
    using ConstSlotSignature = void (T::*)(const std::string &) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const std::string& equation_name) {
            (receiver->*slot)(equation_name);
        };
    }
};

// Queued connection specialization for kEquationRemoved
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoved, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const std::string &);
    using ConstSlotSignature = void (T::*)(const std::string &) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const std::string& equation_name) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation_name]() { (receiver->*slot)(equation_name); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationRemoved (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoved, T, Connection>
{
    using SlotSignature = void (T::*)(const std::string &);
    using ConstSlotSignature = void (T::*)(const std::string &) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const std::string& equation_name) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation_name]() { (receiver->*slot)(equation_name); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationUpdated
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationUpdated, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation, bitmask::bitmask<EquationUpdateFlag> flags) {
            (receiver->*slot)(equation, flags);
        };
    }
};

// Queued connection specialization for kEquationUpdated
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationUpdated, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation, bitmask::bitmask<EquationUpdateFlag> flags) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation, flags]() { (receiver->*slot)(equation, flags); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationUpdated (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationUpdated, T, Connection>
{
    using SlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation, bitmask::bitmask<EquationUpdateFlag> flags) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, equation, flags]() { (receiver->*slot)(equation, flags); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationGroupAdded
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupAdded, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            (receiver->*slot)(group);
        };
    }
};

// Queued connection specialization for kEquationGroupAdded
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupAdded, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group]() { (receiver->*slot)(group); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationGroupAdded (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupAdded, T, Connection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group]() { (receiver->*slot)(group); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationGroupRemoving
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupRemoving, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            (receiver->*slot)(group);
        };
    }
};

// Queued connection specialization for kEquationGroupRemoving
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupRemoving, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group]() { (receiver->*slot)(group); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationGroupRemoving (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupRemoving, T, Connection>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group]() { (receiver->*slot)(group); },
                Connection
            );
        };
    }
};

// Direct connection specialization for kEquationGroupUpdated
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupUpdated, T, Qt::DirectConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group, bitmask::bitmask<EquationGroupUpdateFlag> flags) {
            (receiver->*slot)(group, flags);
        };
    }
};

// Queued connection specialization for kEquationGroupUpdated
template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupUpdated, T, Qt::QueuedConnection>
{
    using SlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group, bitmask::bitmask<EquationGroupUpdateFlag> flags) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group, flags]() { (receiver->*slot)(group, flags); },
                Qt::QueuedConnection
            );
        };
    }
};

// Generic connection specialization for kEquationGroupUpdated (for other connection types)
template<typename T, Qt::ConnectionType Connection>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupUpdated, T, Connection>
{
    using SlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same<Slot, SlotSignature>::value || std::is_same<Slot, ConstSlotSignature>::value;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const EquationGroup* group, bitmask::bitmask<EquationGroupUpdateFlag> flags) {
            QMetaObject::invokeMethod(
                receiver,
                [receiver, slot, group, flags]() { (receiver->*slot)(group, flags); },
                Connection
            );
        };
    }
};

template<EquationEvent Event, typename T, typename Slot, Qt::ConnectionType Connection = Qt::QueuedConnection>
inline void ConnectEquationSignal(
    const EquationSignalsManager* signals_manager,
    T* receiver,
    Slot slot
)
{
    static_assert(std::is_base_of<QObject, T>::value, "receiver must inherit QObject");

    using Traits = EquationQtSignalTraits<Event, T, Connection>;
    static_assert(Traits::template IsValidSlot<Slot>(), "slot signature must match the event");

    auto invoke = Traits::MakeInvoker(receiver, slot);
    auto connection = signals_manager->Connect<Event>(std::move(invoke));

    // Disconnect automatically when receiver is destroyed
    QObject::connect(receiver, &QObject::destroyed, [connection]() mutable {
        if (connection.connected())
        {
            connection.disconnect();
        }
    });
}

// Helper function for direct connection (execute in calling thread)
template<EquationEvent Event, typename T, typename Slot>
inline void ConnectEquationSignalDirect(
    const EquationSignalsManager* signals_manager,
    T* receiver,
    Slot slot
)
{
    ConnectEquationSignal<Event, T, Slot, Qt::DirectConnection>(signals_manager, receiver, slot);
}

} // namespace gui
} // namespace xequation