#include "rtc/base/time/clock_real_time.hpp"
#include "common/utils_time.hpp"

namespace naivertc {
namespace {

// The offset between NTP time and system clock
// 1) Calculate the offset between UTC and system clock
// 2) Convert UTC offset to NTP offset
int64_t NtpOffsetInUs() {
    // Time interval in seconds between 1970 and 1900
    constexpr int64_t kNtpJan1970Sec = 2208988800;
    int64_t clock_time = utils::time::TimeInMicros();
    int64_t utc_time = utils::time::TimeUTCInMicros();
    return (utc_time - clock_time /* Offset between UTC and system clock */) + kNtpJan1970Sec *  kNumMicrosecsPerSec;
}

NtpTime TimeMicrosToNtp(int64_t time_us) {
    static int64_t ntp_offset_us = NtpOffsetInUs();

    int64_t time_ntp_us = time_us + ntp_offset_us;
    // Time before year 1900 is unsupported.
    assert(time_ntp_us >= 0);

    // Convert seconds to uint32 through uint64 for a well-defined cast.
    // A wrap around, which will happen in 2036, is expected for NTP time.
    uint32_t ntp_seconds = static_cast<uint64_t>(time_ntp_us / kNumMicrosecsPerSec);

    // Scale fractions of the second to NTP resolution.
    constexpr int64_t kNtpFractionsInSecond = 1LL << 32;
    int64_t us_fractions = time_ntp_us %  kNumMicrosecsPerSec;
    uint32_t ntp_fractions = us_fractions * kNtpFractionsInSecond / kNumMicrosecsPerSec;

    return NtpTime(ntp_seconds, ntp_fractions);
}
} // namespace

// RealTimeClock implements
RealTimeClock::RealTimeClock() {}

RealTimeClock::~RealTimeClock() {}

Timestamp RealTimeClock::CurrentTime() {
    return Timestamp::Micros(utils::time::TimeInMicros());
}

NtpTime RealTimeClock::ConvertTimestampToNtpTime(Timestamp timestamp) {
    return TimeMicrosToNtp(timestamp.us());
}

} // namespace naivertc
