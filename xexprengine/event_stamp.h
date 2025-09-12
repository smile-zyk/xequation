#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <iostream>


namespace xexprengine
{
class EventStamp
{
  public:
    explicit EventStamp(uint64_t value = 0) : value_(value) {}
    EventStamp(const EventStamp &) = default;
    EventStamp(EventStamp &&) = default;
    EventStamp &operator=(const EventStamp &) = default;
    EventStamp &operator=(EventStamp &&) = default;
    ~EventStamp() = default;

    uint64_t get() const
    {
        return value_;
    }
    explicit operator uint64_t() const
    {
        return value_;
    }

    constexpr bool operator==(const EventStamp &other) const noexcept
    {
        return value_ == other.value_;
    }
    constexpr bool operator!=(const EventStamp &other) const noexcept
    {
        return value_ != other.value_;
    }
    constexpr bool operator<(const EventStamp &other) const noexcept
    {
        return value_ < other.value_;
    }
    constexpr bool operator<=(const EventStamp &other) const noexcept
    {
        return value_ <= other.value_;
    }
    constexpr bool operator>(const EventStamp &other) const noexcept
    {
        return value_ > other.value_;
    }
    constexpr bool operator>=(const EventStamp &other) const noexcept
    {
        return value_ >= other.value_;
    }

  private:
    uint64_t value_;
};

class EventStampGenerator
{
  public:
    EventStampGenerator(const EventStampGenerator &) = delete;
    EventStampGenerator &operator=(const EventStampGenerator &) = delete;
    EventStampGenerator(EventStampGenerator &&) = delete;
    EventStampGenerator &operator=(EventStampGenerator &&) = delete;

    static EventStampGenerator &GetInstance();

    EventStamp GetNextStamp();

    EventStamp GetCurrentStamp() const;

  private:
    EventStampGenerator();
    std::atomic<uint64_t> current_stamp_;
};
} // namespace xexprengine

namespace std
{
template <>
struct hash<xexprengine::EventStamp>
{
    size_t operator()(const xexprengine::EventStamp &es) const noexcept
    {
        return hash<uint64_t>()(es.get());
    }
};
} // namespace std

inline std::ostream &operator<<(std::ostream &os, const xexprengine::EventStamp &es)
{
    os << "EventStamp{" << es.get() << "}";
    return os;
}