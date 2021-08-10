#include "rtc/base/clock.hpp"
#include "rtc/base/clock_real_time.hpp"

#if defined(NAIVERTC_POSIX)
#include <sys/time.h>
#include <time.h>
#endif

namespace naivertc {

class UnixRealTimeClock : public RealTimeClock {
 public:
  UnixRealTimeClock() {}

  ~UnixRealTimeClock() override {}

 protected:
  timeval CurrentTimeVal() {
    struct timeval tv;
    struct timezone tz;
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;
    gettimeofday(&tv, &tz);
    return tv;
  }
};

Clock* Clock::GetRealTimeClock() {
#if defined(NAIVERTC_POSIX)
    static Clock* const clock = new UnixRealTimeClock();
#else
    static Clock* const clock = nullptr;
#endif
    return clock;
}
    
} // namespace naivertc
