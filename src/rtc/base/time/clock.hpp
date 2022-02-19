#ifndef _RTC_BASE_CLOCK_H_
#define _RTC_BASE_CLOCK_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/time/ntp_time.hpp"

namespace naivertc {

// 对于一个系统而言，需要定义一个epoch，所有的时间表示是基于这个基准点的，
// 对于linux而言，采用了和unix epoch一样的时间点：1970年1月1日0点0分0秒（UTC）。
// NTP协议使用的基准点是：1900年1月1日0点0分0秒（UTC）。
// GPS系统使用的基准点是：1980年1月6日0点0分0秒（UTC）。
// 每个系统都可以根据自己的逻辑定义自己epoch，例如unix epoch的基准点是因为unix操作系统是在1970年左右成型的。
// 详见 https://www.cnblogs.com/arnoldlu/p/7078179.html

// Number of seconds since 1900 January 1 00:00 GMT (see
// https://tools.ietf.org/html/rfc868).
constexpr uint32_t kNtpJan1970s = 2'208'988'800UL;
constexpr int64_t kNtpJan1970Ms = 2'208'988'800'000;

// Magic NTP fractional unit
constexpr double kMagicNtpFractionalUnit = 4.294967296E+9;

// A clock interface that allows reading of absolute and relative timestamps
class Clock {
public:
    virtual ~Clock() = default;

    // Return a timestamp relative to an unspecified epoch.
    virtual Timestamp CurrentTime() = 0;

    int64_t now_ms() { return CurrentTime().ms(); }
    int64_t now_us() { return CurrentTime().us(); }

    // Converts between a relative timestamp returned by this clock, to NTP time.
    virtual NtpTime ConvertTimestampToNtpTime(Timestamp timestamp) = 0;
    int64_t ConvertTimestampToNtpTimeInMs(int64_t timestamp_ms) {
        return ConvertTimestampToNtpTime(Timestamp::Millis(timestamp_ms)).ToMs();
    }

    // Retrieve an NTP absolute timestamp (with an epoch of Jan 1, 1900) 
    NtpTime CurrentNtpTime() {
        return ConvertTimestampToNtpTime(CurrentTime());
    }
    int64_t now_ntp_time_ms() {
        return CurrentNtpTime().ToMs();
    }

    // Returns an instance of the real-time system clock implementation.
    static std::unique_ptr<Clock> GetRealTimeClock();
};
    
} // namespace naivertc


#endif