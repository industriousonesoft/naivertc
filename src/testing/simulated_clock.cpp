#include "testing/simulated_clock.hpp"

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

void SimulatedClock::AdvanceTimeMs(int64_t time_ms) {
    AdvanceTime(TimeDelta::Millis(time_ms));
}

void SimulatedClock::AdvanceTimeUs(int64_t time_us) {
    AdvanceTime(TimeDelta::Micros(time_us));
}

// TODO(bugs.webrtc.org(12102): It's desirable to let a single thread own
// advancement of the clock. We could then replace this read-modify-write
// operation with just a thread checker.
void SimulatedClock::AdvanceTime(TimeDelta delta) {
    time_us_.fetch_add(delta.us(), std::memory_order_relaxed);
}

} // namespace naivertc