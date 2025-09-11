#pragma once
#include <atomic>

namespace xexprengine
{
class EventStampGenerator
{
  public:
    EventStampGenerator(const EventStampGenerator &) = delete;
    EventStampGenerator &operator=(const EventStampGenerator &) = delete;

    EventStampGenerator(EventStampGenerator &&) = delete;
    EventStampGenerator &operator=(EventStampGenerator &&) = delete;

    static EventStampGenerator &GetInstance();

    uint64_t GetNextStamp();

    uint64_t GetCurrentStamp() const;

  private:
    EventStampGenerator();
    std::atomic<uint64_t> current_stamp_;
};
} // namespace xexprengine