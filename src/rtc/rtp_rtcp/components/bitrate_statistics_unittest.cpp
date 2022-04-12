#include "rtc/rtp_rtcp/components/bitrate_statistics.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {

using namespace naivertc;

constexpr Timestamp kStartTime = Timestamp::Millis(1000'000); // 1000s
constexpr TimeDelta kDefaultWindowSize = TimeDelta::Millis(500);  // 500ms

class T(BitrateStatisticsTest) : public ::testing::Test {
protected:
    T(BitrateStatisticsTest)() 
        : stats_(kDefaultWindowSize) {};
protected:
    BitrateStatistics stats_;
};

MY_TEST_F(BitrateStatisticsTest, TestStrictMode) {
    Timestamp now = kStartTime;
    EXPECT_FALSE(stats_.Rate(now).has_value());

    const uint32_t kPacketSize = 1500u;
    const uint32_t kExpectedRateBps = kPacketSize * 1000 * 8;

    // Single data point is not enough for valid estimate.
    stats_.Update(kPacketSize, now);
    now += TimeDelta::Millis(1);
    EXPECT_FALSE(stats_.Rate(now).has_value());

    // Expecting 1200 kbps since the window is initially kept small and grows as
    // we have more data.
    stats_.Update(kPacketSize, now);
    auto bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(kExpectedRateBps, bitrate->bps());

    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now).has_value());

    const int kInterval = 10;
    for (int i = 0; i < 100000; ++i) {
        if (i % kInterval == 0)
            stats_.Update(kPacketSize, now);

        // Approximately 1200 kbps expected. Not exact since when packets
        // are removed we will jump 10 ms to the next packet.
        if (i > kInterval) {
            std::optional<DataRate> bitrate = stats_.Rate(now);
            EXPECT_TRUE(bitrate.has_value());
            uint32_t samples = i / kInterval + 1;
            uint64_t total_bits = samples * kPacketSize * 8;
            uint32_t rate_bps = static_cast<uint32_t>((1000 * total_bits) / (i + 1));
            EXPECT_NEAR(rate_bps, bitrate->bps(), 22000u);
        }
        now += TimeDelta::Millis(1);
    }
    now += kDefaultWindowSize;
    // The window is 2 seconds. If nothing has been received for that time
    // the estimate should be 0.
    bitrate = stats_.Rate(now);
    EXPECT_FALSE(bitrate.has_value());
    EXPECT_EQ(stats_.accumulated_bytes(), 0);
    EXPECT_EQ(stats_.num_samples(), 0);
    EXPECT_EQ(stats_.num_bucket(), 0);
}

MY_TEST_F(BitrateStatisticsTest, IncreasingThenDecreasingBitrate) {
    Timestamp now = kStartTime;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now)));

    now += TimeDelta::Millis(1);
    stats_.Update(1000, now);
    // 8000 kbs
    const uint32_t kExpectedBitrateBps = 8000000;
    // 1000 bytes per millisecond until plateau is reached.
    int prev_delta = kExpectedBitrateBps;
    std::optional<DataRate> bitrate;

    now += TimeDelta::Millis(1);
    auto end = now + TimeDelta::Seconds(10);
    // 1000 bytes per millisecond until plateau is reached.
    while (now < end) {
        stats_.Update(1000, now);
        bitrate = stats_.Rate(now);
        EXPECT_LE(stats_.num_bucket(), 501);
        EXPECT_TRUE(bitrate.has_value());
        int delta = kExpectedBitrateBps - bitrate->bps();
        delta = std::abs(delta);
        // Expect the estimation delta to decrease as the window is extended.
        EXPECT_LE(delta, prev_delta + 1);
        prev_delta = delta;
        now += TimeDelta::Millis(1);
    }
    
    // Window filled, expect to be close to 8000000.
    EXPECT_EQ(kExpectedBitrateBps, bitrate->bps());

    // 1000 bytes per millisecond until 10-second mark, 8000 kbps expected.
    end = now + TimeDelta::Seconds(10);
    while (now < end) {
        stats_.Update(1000, now);
        bitrate = stats_.Rate(now);
        EXPECT_EQ(kExpectedBitrateBps, bitrate->bps());
        now += TimeDelta::Millis(1);
    }

    // Zero bytes per millisecond until 0 is reached.
    end = now + TimeDelta::Seconds(20);
    std::optional<DataRate> new_bitrate;
    while (now < end) {
        stats_.Update(0, now);
        new_bitrate = stats_.Rate(now);
        if (static_cast<bool>(new_bitrate) && new_bitrate->bps() != bitrate->bps()) {
            // New bitrate must be lower than previous one.
            EXPECT_LT(new_bitrate->bps(), bitrate->bps());
        } else {
            // 0 kbps expected.
            EXPECT_EQ(0u, new_bitrate->bps());
            break;
        }
        bitrate = new_bitrate;
        now += TimeDelta::Millis(1);
    }
}


MY_TEST_F(BitrateStatisticsTest, ResetAfterSilence) {
    Timestamp now = kStartTime;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now)));

    // 8000 kbps
    const uint32_t kExpectedBitrate = 8000000;
    // 1000 bytes per millisecond until the window has been filled.
    int prev_delta = kExpectedBitrate;
    std::optional<DataRate> bitrate;

    auto end = now + TimeDelta::Seconds(10);
    while (now < end) {
        stats_.Update(1000, now);
        bitrate = stats_.Rate(now);
        if (bitrate.has_value()) {
            int delta = kExpectedBitrate - bitrate->bps();
            delta = std::abs(delta);
            // Expect the estimation delta to decrease as the window is extended.
            EXPECT_LE(delta, prev_delta + 1);
            prev_delta = delta;
        }
        now += TimeDelta::Millis(1);
    }
    // Window filled, expect to be close to 8000000.
    EXPECT_EQ(kExpectedBitrate, bitrate->bps());

    // Silience over window size.
    now += kDefaultWindowSize + TimeDelta::Millis(1);
    EXPECT_FALSE(stats_.Rate(now));

    // Silence over window size should trigger auto reset for coming sample.
    stats_.Update(1000, now);
    now += TimeDelta::Millis(1);
    stats_.Update(1000, now);
    // We expect two samples of 1000 bytes, and that the bitrate is measured over
    // active window instead of full window, which is now_ms - first_timestamp + 1
    EXPECT_EQ(kExpectedBitrate, stats_.Rate(now)->bps());

    // Reset, add the same samples again.
    stats_.Reset();
    EXPECT_FALSE(stats_.Rate(now).has_value());

    stats_.Update(1000, now);
    now += TimeDelta::Millis(1);
    stats_.Update(1000, now);
    // We expect two samples of 1000 bytes, and that the bitrate is measured over
    // 2 ms (window size has been reset) i.e. 2 * 8 * 1000 / 0.002 = 8000000.
    EXPECT_EQ(kExpectedBitrate, stats_.Rate(now)->bps());
}

MY_TEST_F(BitrateStatisticsTest, HandlesChangingWindowSize) {
    auto now = kStartTime;
    stats_.Reset();

    // Sanity test window size.
    EXPECT_TRUE(stats_.SetWindowSize(kDefaultWindowSize, now));
    EXPECT_FALSE(stats_.SetWindowSize(kDefaultWindowSize + TimeDelta::Millis(1), now));
    EXPECT_FALSE(stats_.SetWindowSize(TimeDelta::Zero(), now));
    EXPECT_TRUE(stats_.SetWindowSize(TimeDelta::Millis(1), now));
    EXPECT_TRUE(stats_.SetWindowSize(kDefaultWindowSize, now));

    // Fill the buffer at a rate of 1 byte / millisecond (8 kbps).
    const size_t kBatchSize = 10;
    const TimeDelta kBatchInterval = TimeDelta::Millis(10);
    for (TimeDelta i = TimeDelta::Zero(); i <= kDefaultWindowSize; i += kBatchInterval) {
        now += kBatchInterval;
        stats_.Update(kBatchSize, now);
    }
    
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now)->bps());

    // Halve the window size, rate should stay the same.
    EXPECT_TRUE(stats_.SetWindowSize(kDefaultWindowSize / 2, now));
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now)->bps());

    // Double the window size again, rate should stay the same. (As the window
    // won't actually expand until new bit and bobs fall into it.
    EXPECT_TRUE(stats_.SetWindowSize(kDefaultWindowSize, now));
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now)->bps());

    // Fill the now empty half with bits it twice the rate.
    for (TimeDelta i = TimeDelta::Zero(); i < kDefaultWindowSize / 2; i += kBatchInterval) {
        now += kBatchInterval;
        stats_.Update(kBatchSize * 2, now);
    }

    // Rate should have increase be 50%.
    EXPECT_EQ(static_cast<uint32_t>((8000 * 3) / 2), stats_.Rate(now)->bps());
}

MY_TEST_F(BitrateStatisticsTest, RespectsWindowSizeEdges) {
    Timestamp now = kStartTime;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now)));

    const size_t kBytes = 500;
    // One byte per ms, using one big sample.
    stats_.Update(kBytes, now);
    now += kDefaultWindowSize - TimeDelta::Millis(2);
    // Shouldn't work! (Only one sample, not full window size.)
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now)));

    // Window size should be full, and the single data point should be accepted.
    now += TimeDelta::Millis(1);
    std::optional<DataRate> bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());

    // Add another, now we have twice the bitrate.
    stats_.Update(kBytes, now);
    bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(2 * 1000 * 8u, bitrate->bps());

    // Now that first sample should drop out...
    now += TimeDelta::Millis(1);
    bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());
}

MY_TEST_F(BitrateStatisticsTest, HandlesZeroCounts) {
    auto now = kStartTime;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now).has_value());

    const size_t kBytes = 500;
    stats_.Update(kBytes, now);
    now += kDefaultWindowSize - TimeDelta::Millis(1);
    stats_.Update(0, now);
    std::optional<DataRate> bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());

    // Move window along so first data point falls out.
    now += TimeDelta::Millis(1);
    bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());

    // Move window so last data point falls out.
    now += kDefaultWindowSize;
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());
}

MY_TEST_F(BitrateStatisticsTest, HandlesQuietPeriods) {
    Timestamp now = kStartTime;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now).has_value());

    stats_.Update(0, now);
    now += kDefaultWindowSize - TimeDelta::Millis(1);
    std::optional<DataRate> bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());

    // Move window along so first data point falls out.
    now += TimeDelta::Millis(1);
    EXPECT_FALSE(stats_.Rate(now).has_value());

    // Move window a long way out.
    now += 2 * kDefaultWindowSize;
    stats_.Update(0, now);
    EXPECT_FALSE(stats_.Rate(now).has_value());

    // Second data point gives valid result
    now += TimeDelta::Millis(1);
    stats_.Update(0, now);
    bitrate = stats_.Rate(now);
    EXPECT_TRUE(static_cast<bool>(bitrate));
    EXPECT_EQ(0u, bitrate->bps());
}

MY_TEST_F(BitrateStatisticsTest, HandlesBigNumbers) {
    size_t large_number = 0x100000000u;
    Timestamp now = kStartTime;
    stats_.Update(large_number, now);
    now += TimeDelta::Millis(1);
    stats_.Update(large_number, now);
    std::optional<DataRate> bitrate = stats_.Rate(now);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(large_number * 8000, bitrate->bps());
}

MY_TEST_F(BitrateStatisticsTest, HandlesTooLargeNumbers) {
    int64_t very_large_number = std::numeric_limits<int64_t>::max();
    Timestamp now = kStartTime;
    stats_.Update(very_large_number, now);
    now += TimeDelta::Millis(1);
    stats_.Update(very_large_number, now);
    // This should overflow the internal accumulator.
    EXPECT_FALSE(stats_.Rate(now).has_value());
}

MY_TEST_F(BitrateStatisticsTest, HandlesSomewhatLargeNumbers) {
    int64_t very_large_number = std::numeric_limits<int64_t>::max();
    Timestamp now = kStartTime;
    stats_.Update(very_large_number / 4, now);
    now += TimeDelta::Millis(1);
    stats_.Update(very_large_number / 4, now);
    // This should generate a rate of more than int64_t max, but still
    // accumulate less than int64_t overflow.
    EXPECT_FALSE(stats_.Rate(now).has_value());
}
    
} // namespace test    
} // namespace naivertc
