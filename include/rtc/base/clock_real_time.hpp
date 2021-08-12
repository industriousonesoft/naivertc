#ifndef _RTC_BASE_CLOCK_REAK_TIME_H_
#define _RTC_BASE_CLOCK_REAK_TIME_H_

#include "base/defines.hpp"
#include "rtc/base/clock.hpp"
#include "rtc/base/ntp_time.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RealTimeClock : public Clock {
public:
    RealTimeClock();
    ~RealTimeClock();

    Timestamp CurrentTime() override;
    NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) override;

};
    
} // namespace naivertc


#endif