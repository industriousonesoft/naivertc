#include "rtc/base/clock_real_time.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(ClockTest, NtpTime) {
    auto clock = Clock::GetRealTimeClock();

    EXPECT_NE(clock, nullptr);

    // To ensure the test runs correctly even on a heaily loaded system,
    // do not compare the seconds/fractions and millisecond values directly.
    // Instread, we check that the NTP time is between the "milliseconds"
    // values returned right before and right after the call.
    // The comparison includes 1 ms of margin to account for the rounding error
    // in the conversion.
    int64_t milliseconds_lower_bound = clock->CurrentNtpTimeInMs();
    NtpTime ntp_time = clock->CurrentNtpTime();
    int64_t milliseconds_upper_bound = clock->CurrentNtpTimeInMs();
    EXPECT_GT(milliseconds_lower_bound / 1000, kNtpJan1970);
    EXPECT_LE(milliseconds_lower_bound - 1, ntp_time.ToMs());
    EXPECT_GE(milliseconds_upper_bound + 1, ntp_time.ToMs());
}

}
} // namespace naivertc