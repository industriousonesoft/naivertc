#ifndef _RTC_RTP_RTCP_TIME_UTIL_H_
#define _RTC_RTP_RTCP_TIME_UTIL_H_

#include "base/defines.hpp"
#include "rtc/base/ntp_time.hpp"

namespace naivertc {

// Helper function for compact ntp representation:
// RFC 3550, Section 4. Time Format.
// Wallclock time is represented using the timestamp format of
// the Network Time Protocol (NTP).
// ...
// In some fields where a more compact representation is
// appropriate, only the middle 32 bits are used; that is, the low 16
// bits of the integer part and the high 16 bits of the fractional part.
inline uint32_t CompactNtp(NtpTime ntp) {
    return (ntp.seconds() << 16) | (ntp.fractions() >> 16);
}

// Converts interval in microseconds to compact ntp (1/2^16 seconds) resolution.
// Negative values converted to 0, Overlarge values converted to max uint32_t.
uint32_t SaturatedUsToCompactNtp(int64_t us);

// Converts interval between compact ntp timestamps to milliseconds.
// This interval can be up to ~9.1 hours (2^15 seconds).
// Values close to 2^16 seconds consider negative and result in minimum rtt = 1.
int64_t CompactNtpRttToMs(uint32_t compact_ntp_interval);
    
} // namespace naivertc


#endif