#include "base/system_time.hpp"
#include "common/utils_numeric.hpp"

#include <stdint.h>
#include <limits>

#if defined(NAIVERTC_POSIX)
#include <sys/time.h>
#if defined(NAIVERTC_MAC)
#include <mach/mach_time.h>
#endif
#endif

namespace naivertc {

int64_t SystemTimeInNanos() {
    int64_t ticks = -1;
#if defined(NAIVERTC_MAC)
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        // Get the timebase if this is the first we run.
        // Recommond by Apple's QA1398
        assert(mach_timebase_info(&timebase) == KERN_SUCCESS && "NaiveRTC not reached.");
    }
    // Use timebase to convert absolute time tick uints into nanoseconds
    const auto mul = [](uint64_t a, uint32_t b) -> int64_t {
        assert(b != 0);
        assert(a <= std::numeric_limits<int64_t>::max() / b);
        return utils::numeric::checked_static_cast<int64_t>(a * b);
    };
    ticks = mul(mach_absolute_time(), timebase.numer) / timebase.denom;
#elif defined(NAIVERTC_POSIX)
    struct timespec ts;
    // TODO: Do we need to handle the case when CLOCK_MONOTONIC is not supported?
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ticks = kNumNanosecsPerSec * static_cast<int64_t>(ts.tv_sec) +
          static_cast<int64_t>(ts.tv_nsec);
#else
    #error Unsupported platform
#endif
    return ticks;
}
    
} // namespace naivertc
