#ifndef _RTC_BASE_TIME_CLOCK_REAK_TIME_H_
#define _RTC_BASE_TIME_CLOCK_REAK_TIME_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/base/time/ntp_time.hpp"

namespace naivertc {

class RealTimeClock : public Clock {
public:
    RealTimeClock();
    ~RealTimeClock();

    Timestamp CurrentTime() override;
    NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) override;

};
    
} // namespace naivertc


#endif