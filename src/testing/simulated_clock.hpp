#ifndef _RTC_BASE_TIME_CLOCK_SIMULATED_H_
#define _RTC_BASE_TIME_CLOCK_SIMULATED_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/units/timestamp.hpp"

#include <atomic>

namespace naivertc {

// This class is thread-safety (being protected atomically).
class RTC_CPP_EXPORT SimulatedClock : public Clock {
public:
    // The constructors assume an epoch of Jan 1, 1970.
    explicit SimulatedClock(int64_t initial_time_us);
    explicit SimulatedClock(Timestamp initial_time);
    ~SimulatedClock() override;

    // Return a timestamp with an epoch of Jan 1, 1970.
    Timestamp CurrentTime() override;

    NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) override;

    // Advance the simulated clock with a given number of milliseconds or
    // microseconds.
    void AdvanceTimeMs(int64_t time_ms);
    void AdvanceTimeUs(int64_t time_us);
    void AdvanceTime(TimeDelta delta);

private:
    // The time is read and incremented with relaxed order. Each thread will see
    // monotonically increasing time, and when threads post tasks or messages to
    // one another, the synchronization done as part of the message passing should
    // ensure that any causual chain of events on multiple threads also
    // corresponds to monotonically increasing time.
    std::atomic<int64_t> time_us_;
};

} // namespace naivertc

#endif