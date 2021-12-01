#include "rtc/base/time/clock.hpp"
#include "rtc/base/time/clock_real_time.hpp"

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

std::unique_ptr<Clock> Clock::GetRealTimeClock() {
#if defined(NAIVERTC_POSIX)
    return std::make_unique<UnixRealTimeClock>();
#else
    return nullptr;
#endif
}
    
} // namespace naivertc
