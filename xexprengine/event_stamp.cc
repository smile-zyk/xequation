#include "event_stamp.h"

using namespace xexprengine;

inline EventStampGenerator &EventStampGenerator::GetInstance() {
  static EventStampGenerator instance;
  return instance;
}

inline EventStamp EventStampGenerator::GetNextStamp() {
  return EventStamp(++current_stamp_);
}

inline EventStamp EventStampGenerator::GetCurrentStamp() const {
  return EventStamp(current_stamp_.load());
}