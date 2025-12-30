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

template<EquationEvent Event, typename T>
struct EquationQtSignalTraits;

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationAdded, T>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoving, T>
{
    using SlotSignature = void (T::*)(const Equation *);
    using ConstSlotSignature = void (T::*)(const Equation *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
    }
    template<typename Slot>
    static auto MakeInvoker(T* receiver, Slot slot)
    {
        return [receiver, slot](const Equation* equation) {
            QMetaObject::invokeMethod(
                receiver    ,
                [receiver, slot, equation]() { (receiver->*slot)(equation); },
                Qt::QueuedConnection
            );
        };
    }
};

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationRemoved, T>
{
    using SlotSignature = void (T::*)(const std::string &);
    using ConstSlotSignature = void (T::*)(const std::string &) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationUpdated, T>
{
    using SlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const Equation *, bitmask::bitmask<EquationUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupAdded, T>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupRemoving, T>
{
    using SlotSignature = void (T::*)(const EquationGroup *);
    using ConstSlotSignature = void (T::*)(const EquationGroup *) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<typename T>
struct EquationQtSignalTraits<EquationEvent::kEquationGroupUpdated, T>
{
    using SlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>);
    using ConstSlotSignature = void (T::*)(const EquationGroup *, bitmask::bitmask<EquationGroupUpdateFlag>) const;
    template<typename Slot>
    static constexpr bool IsValidSlot()
    {
        return std::is_same_v<Slot, SlotSignature> || std::is_same_v<Slot, ConstSlotSignature>;
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

template<EquationEvent Event, typename T, typename Slot>
inline void ConnectEquationSignal(
    const EquationSignalsManager* signals_manager,
    T* receiver,
    Slot slot
)
{
    static_assert(std::is_base_of_v<QObject, T>, "receiver must inherit QObject");

    using Traits = EquationQtSignalTraits<Event, T>;
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

} // namespace gui
} // namespace xequation