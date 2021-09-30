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

std::shared_ptr<Clock> Clock::GetRealTimeClock() {
#if defined(NAIVERTC_POSIX)
    static std::shared_ptr<Clock> clock = std::make_shared<UnixRealTimeClock>();
#else
    static std::shared_ptr<Clock> clock = nullptr;
#endif
    return clock;
}
    
} // namespace naivertc
