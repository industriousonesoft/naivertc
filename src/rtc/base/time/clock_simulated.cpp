#include "rtc/base/time/clock_simulated.hpp"

namespace naivertc {

SimulatedClock::SimulatedClock(int64_t initial_time_us)
    : time_us_(initial_time_us) {}

SimulatedClock::SimulatedClock(Timestamp initial_time)
    : SimulatedClock(initial_time.us()) {}

SimulatedClock::~SimulatedClock() {}

Timestamp SimulatedClock::CurrentTime() {
    return Timestamp::Micros(time_us_.load(std::memory_order_relaxed));
}

NtpTime SimulatedClock::ConvertTimestampToNtpTime(Timestamp timestamp) {
    int64_t now_us = timestamp.us();
    uint32_t seconds = (now_us / 1'000'000) + kNtpJan1970;
    uint32_t fractions = static_cast<uint32_t>(
        (now_us % 1'000'000) * kMagicNtpFractionalUnit / 1'000'000);
    return NtpTime(seconds, fractions);
}

void SimulatedClock::AdvanceTimeMilliseconds(int64_t milliseconds) {
    AdvanceTime(TimeDelta::Millis(milliseconds));
}

void SimulatedClock::AdvanceTimeMicroseconds(int64_t microseconds) {
    AdvanceTime(TimeDelta::Micros(microseconds));
}

void SimulatedClock::AdvanceTime(TimeDelta delta) {
    time_us_.fetch_add(delta.us(), std::memory_order_relaxed);
}

} // namespace naivertc