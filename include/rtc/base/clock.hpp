#ifndef _RTC_BASE_CLOCK_H_
#define _RTC_BASE_CLOCK_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/ntp_time.hpp"

namespace naivertc {

// January 1970, in NTP seconds
// Time interval in seconds between 1970 and 1900
constexpr uint32_t kNtpJan1970 = 2208988800UL;

// Magic NTP fractional unit
constexpr double kMagicNtpFractionalUnit = 4.294967296E+9;

// A clock interface that allows reading of absolute and relative timestamps
class RTC_CPP_EXPORT Clock {
public:
    virtual ~Clock() = default;

    // Return a timestamp relative to an unspecified epoch.
    virtual Timestamp CurrentTime() = 0;

    int64_t TimeInMs() { return CurrentTime().ms(); }
    int64_t TimeInUs() { return CurrentTime().us(); }

    // Converts between a relative timestamp returned by this clock, to NTP time.
    virtual NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) = 0;
    int64_t ConvertTimestampToNtpTimeInMs(int64_t timestamp_ms) {
        return ConvertTimestampToNtpTime(Timestamp::Millis(timestamp_ms)).ToMs();
    }

    // Retrieve an NTP absolute timestamp (with an epoch of Jan 1, 1900) 
    NtpTime CurrentNtpTime() {
        return ConvertTimestampToNtpTime(CurrentTime());
    }
    int64_t CurrentNtpTimeInMs() {
        return CurrentNtpTime().ToMs();
    }

    // Returns an instance of the real-time system clock implementation.
    static std::shared_ptr<Clock> GetRealTimeClock();
};
    
} // namespace naivertc


#endif