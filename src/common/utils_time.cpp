#include "common/utils_time.hpp"
#include "base/system_time.hpp"

#if defined(NAIVERTC_POSIX)
#include <sys/time.h>
#endif

namespace naivertc {
namespace utils {
namespace time {

// Returns the current time in milliseconds in 32 bits.
uint32_t Time32InMillis() {
    return static_cast<uint32_t>(TimeInNanos() / kNumNanosecsPerMillisec);
}   

// Returns the current time in milliseconds in 64 bits.
int64_t TimeInMillis() {
    return TimeInNanos() / kNumNanosecsPerMillisec;
}

// Returns the current time in microseconds in 64 bits.
int64_t TimeInMicros() {
    return TimeInNanos() / kNumNanosecsPerMicrosec;
}

// Returns the current time in nanoseconds in 64 bits.
int64_t TimeInNanos() {
    return SystemTimeInNanos();
}

int64_t TimeUTCInMicros() {
#if defined(NAIVERTC_POSIX)
    struct timeval time;
    gettimeofday(&time, nullptr);
    // Convert from second (1.0) and microsecond (1e-6).
    return static_cast<int64_t>(time.tv_sec) * kNumMicrosecsPerSec + time.tv_usec; 
#else
    #error "Unsupported plotform";
#endif
}
    
} // namespace time
} // namespace utils
} // namespace naivertc
