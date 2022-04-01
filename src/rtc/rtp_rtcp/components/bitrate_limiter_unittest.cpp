#include "rtc/rtp_rtcp/components/bitrate_limiter.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr TimeDelta kWindowSize = TimeDelta::Millis(1000);
constexpr DataRate kMaxBitrate = DataRate::BitsPerSec(100'000);
// Bytes needed to completely saturate the bitrate limiter.
constexpr size_t kBitrateFilllingBytes = kMaxBitrate * kWindowSize;

} // namespace


class T(BitrateLimiterTest) : public ::testing::Test {
public:
    T(BitrateLimiterTest)() 
        : clock_(12345678),
          bitrate_limiter_(&clock_, kWindowSize) {}
    ~T(BitrateLimiterTest)() override {}

    void SetUp() override {
        bitrate_limiter_.set_max_bitrate(kMaxBitrate);
    }

protected:
    SimulatedClock clock_;
    BitrateLimiter bitrate_limiter_;
};

MY_TEST_F(BitrateLimiterTest, IncreasingMaxRate) {
    // Fill bitrate, extend window to full size
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));
    clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));

    // All bitrate has consumed already.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));

    // Increase bitrate by doubling the available bitrate.
    bitrate_limiter_.set_max_bitrate(kMaxBitrate * 2);
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes));

    // All bitrate has consumed already.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));
}

MY_TEST_F(BitrateLimiterTest, DecreasingMaxRate) {
    // Fill bitrate, extend window to full size
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));
    clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));

    // All bitrate has consumed already.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));

    // Decrease bitrate by havling the available bitrate.
    bitrate_limiter_.set_max_bitrate(kMaxBitrate / 2);
    // Move window so half of the data falls out.
    clock_.AdvanceTimeMs(1);

    // All bitrate has consumed already.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));
}

MY_TEST_F(BitrateLimiterTest, ChangingWindowSize) {
    // Fill rate, extend window to full size.
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));
    clock_.AdvanceTime(kWindowSize - TimeDelta::Millis(1));
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));

    // All rate consumed.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));

    // Decrease window size so half of the data falls out.
    bitrate_limiter_.SetWindowSize(kWindowSize / 2);
    // Average rate should still be the same, so rate is still all consumed.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));

    // Increase window size again. Now the rate is only half used (removed data
    // points don't come back to life).
    bitrate_limiter_.SetWindowSize(kWindowSize);
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes / 2));

    // All rate consumed again.
    EXPECT_FALSE(bitrate_limiter_.TryConsumeBitrate(1));
}

MY_TEST_F(BitrateLimiterTest, SingleUsageAlwaysOk) {
    // Using more bytes than can fit in a window is OK for a single packet.
    EXPECT_TRUE(bitrate_limiter_.TryConsumeBitrate(kBitrateFilllingBytes + 1));
}

MY_TEST_F(BitrateLimiterTest, WindowSizeLimits) {
    EXPECT_TRUE(bitrate_limiter_.SetWindowSize(TimeDelta::Millis(1)));
    EXPECT_FALSE(bitrate_limiter_.SetWindowSize(TimeDelta::Millis(0)));
    EXPECT_TRUE(bitrate_limiter_.SetWindowSize(kWindowSize));
    EXPECT_FALSE(bitrate_limiter_.SetWindowSize(kWindowSize + TimeDelta::Millis(1)));
}
    
} // namespace test
} // namespace naivertc
