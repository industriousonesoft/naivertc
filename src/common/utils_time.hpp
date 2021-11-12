#ifndef _BASE_TIME_UTILS_H_
#define _BASE_TIME_UTILS_H_

#include "base/defines.hpp"

#include <stdint.h>

namespace naivertc {

static constexpr int64_t kNumMillisecsPerSec = INT64_C(1000);
static constexpr int64_t kNumMicrosecsPerSec = INT64_C(1000000);
static constexpr int64_t kNumNanosecsPerSec = INT64_C(1000000000);

static constexpr int64_t kNumMicrosecsPerMillisec = kNumMicrosecsPerSec / kNumMillisecsPerSec;
static constexpr int64_t kNumNanosecsPerMillisec = kNumNanosecsPerSec / kNumMillisecsPerSec;
static constexpr int64_t kNumNanosecsPerMicrosec = kNumNanosecsPerSec / kNumMicrosecsPerSec;

namespace utils {
namespace time {

// Returns the current time in seconds in 64 bits.
RTC_CPP_EXPORT int64_t TimeInSec();

// Returns the current time in milliseconds in 32 bits.
RTC_CPP_EXPORT uint32_t Time32InMillis();

// Returns the current time in milliseconds in 64 bits.
RTC_CPP_EXPORT int64_t TimeInMillis();

// Returns the current time in microseconds in 64 bits.
RTC_CPP_EXPORT int64_t TimeInMicros();

// Returns the current time in nanoseconds in 64 bits.
RTC_CPP_EXPORT int64_t TimeInNanos();

// Returns the number of microseconds since January 1, 1970, UTC.
// Useful mainly when producing logs to be corrected with other devices,
// and when the devices in question all have properly synchronized clocks.
//
// Note that this function obeys the system's idea about what the time is.
// It is not guarantted to be monotomic; it will jump in case the system 
// time in changed, e.g., by some other process calling settimeofday. 
// Always use TimeMicros(), not this function, for measuring time intervals 
// and timeouts.
RTC_CPP_EXPORT int64_t TimeUTCInMicros();
    
} // namespace time
} // namespace utils
} // namespace naivertc

#endif