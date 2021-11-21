#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timestamp_extrapolator.hpp"
#include "rtc/base/time/clock_simulated.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

constexpr int kFps = 25;

class T(TimestampExtrapolatorTest) : public ::testing::Test {
public:
    T(TimestampExtrapolatorTest)() : clock_(0), extrapolator_(clock_.now_ms()) {}
protected:
    SimulatedClock clock_;
    naivertc::rtp::video::TimestampExtrapolator extrapolator_;
};

MY_TEST_F(TimestampExtrapolatorTest, TimestampWrapAround) {
    extrapolator_.Reset(clock_.now_ms());
    uint32_t ts_interval = 90'000 / kFps;
    int time_interval = 1'000 / kFps;
    // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
    uint32_t timestamp = 0xFFFFFFFFu - 3 * ts_interval;
    for (int i = 0; i < 5; ++i) {
        extrapolator_.Update(timestamp, clock_.now_ms() /* receiver_time_ms */);
        clock_.AdvanceTimeMs(time_interval);
        timestamp += ts_interval;
        EXPECT_EQ(3 * time_interval, extrapolator_.ExtrapolateLocalTime(0xFFFFFFFFu));
        EXPECT_EQ(3 * time_interval + 1, extrapolator_.ExtrapolateLocalTime(89u) /* test round up */);
        EXPECT_EQ(3 * time_interval + 1 /* step = 1 */, extrapolator_.ExtrapolateLocalTime(90u) /* step = 90 */);
        EXPECT_EQ(3 * time_interval + 2, extrapolator_.ExtrapolateLocalTime(180u));
    }
}

} // namespace test
} // namespace naivertc 