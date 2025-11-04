#include "event_stamp.h"
#include <cstdint>

using namespace xequation;

EventStampGenerator::EventStampGenerator() : current_stamp_(0) {}

EventStampGenerator &EventStampGenerator::GetInstance()
{
    static EventStampGenerator instance;
    return instance;
}

EventStamp EventStampGenerator::GetNextStamp()
{
    return EventStamp(++current_stamp_);
}

EventStamp EventStampGenerator::GetCurrentStamp() const
{
    return EventStamp(current_stamp_.load());
}

EventStamp EventStampGenerator::GetMaxStamp() const
{
    static EventStamp max_stamp(UINT64_MAX);
    return max_stamp;
}