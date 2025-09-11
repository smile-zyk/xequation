#include "event_stamp_generator.h"

using namespace xexprengine;

EventStampGenerator::EventStampGenerator(): current_stamp_(0) {}

EventStampGenerator& EventStampGenerator::GetInstance() {
    static EventStampGenerator instance; 
    return instance;
}

uint64_t EventStampGenerator::GetNextStamp() {
    return current_stamp_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t EventStampGenerator::GetCurrentStamp() const {
    return current_stamp_.load(std::memory_order_relaxed);
}