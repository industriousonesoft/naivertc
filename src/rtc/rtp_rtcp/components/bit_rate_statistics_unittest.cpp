#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

using namespace naivertc;

constexpr int64_t KWindowSizeMs = 500; 

class BitRateStatisticsTest : public ::testing::Test {
protected:
    BitRateStatisticsTest() : stats_(KWindowSizeMs) {};
    BitRateStatistics stats_;
};

TEST_F(BitRateStatisticsTest, TestStrictMode) {
    int64_t now_ms = 0;
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    const uint32_t kPacketSize = 1500u;
    const uint32_t kExpectedRateBps = kPacketSize * 1000 * 8;

    // Single data point is not enough for valid estimate.
    stats_.Update(kPacketSize, now_ms++);
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    // Expecting 1200 kbps since the window is initially kept small and grows as
    // we have more data.
    stats_.Update(kPacketSize, now_ms);
    auto bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(kExpectedRateBps, bitrate->bps());

    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    const int kInterval = 10;
    for (int i = 0; i < 100000; ++i) {
        if (i % kInterval == 0)
            stats_.Update(kPacketSize, now_ms);

        // Approximately 1200 kbps expected. Not exact since when packets
        // are removed we will jump 10 ms to the next packet.
        if (i > kInterval) {
            std::optional<BitRate> bitrate = stats_.Rate(now_ms);
            EXPECT_TRUE(bitrate.has_value());
            uint32_t samples = i / kInterval + 1;
            uint64_t total_bits = samples * kPacketSize * 8;
            uint32_t rate_bps = static_cast<uint32_t>((1000 * total_bits) / (i + 1));
            EXPECT_NEAR(rate_bps, bitrate->bps(), 22000u);
        }
        now_ms += 1;
    }
    now_ms += KWindowSizeMs;
    // The window is 2 seconds. If nothing has been received for that time
    // the estimate should be 0.
    bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(bitrate->bps(), 0);
    EXPECT_EQ(stats_.total_accumulated_bytes(), 0);
    EXPECT_EQ(stats_.total_num_samples(), 0);
    EXPECT_EQ(stats_.num_bucket(), 0);
}

TEST_F(BitRateStatisticsTest, IncreasingThenDecreasingBitrate) {
    int64_t now_ms = 0;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now_ms)));

    stats_.Update(1000, ++now_ms);
    // 8000 kbs
    const uint32_t kExpectedBps = 8000000;
    // 1000 bytes per millisecond until plateau is reached.
    int prev_delta = kExpectedBps;
    EXPECT_EQ(prev_delta, kExpectedBps);
    std::optional<BitRate> bitrate;
    while (++now_ms < 10000) {
        stats_.Update(1000, now_ms);
        bitrate = stats_.Rate(now_ms);
        EXPECT_LE(stats_.num_bucket(), 501);
        EXPECT_TRUE(static_cast<bool>(bitrate));
        int delta = kExpectedBps - bitrate->bps();
        delta = std::abs(delta);
        // Expect the estimation delta to decrease as the window is extended.
        EXPECT_LE(delta, prev_delta + 1);
        prev_delta = delta;
    }
    
    // Window filled, expect to be close to 8000000.
    EXPECT_EQ(kExpectedBps, bitrate->bps());

    // Zero bytes per millisecond until 0 is reached.
    std::optional<BitRate> new_bitrate;
    while (++now_ms < 20000) {
        stats_.Update(0, now_ms);
        new_bitrate = stats_.Rate(now_ms);
        if (static_cast<bool>(new_bitrate) && new_bitrate->bps() != bitrate->bps()) {
            // New bitrate must be lower than previous one.
            EXPECT_LT(new_bitrate->bps(), bitrate->bps());
        } else {
            // 0 kbps expected.
            EXPECT_EQ(0u, new_bitrate->bps());
            break;
        }
        bitrate = new_bitrate;
    }
}


TEST_F(BitRateStatisticsTest, ResetAfterSilence) {
    int64_t now_ms = 0;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now_ms)));

    // 8000 kbps
    const uint32_t kExpectedBitrate = 8000000;
    // 1000 bytes per millisecond until the window has been filled.
    int prev_delta = kExpectedBitrate;
    std::optional<BitRate> bitrate;
    while (++now_ms < 10000) {
        stats_.Update(1000, now_ms);
        bitrate = stats_.Rate(now_ms);
        if (bitrate.has_value()) {
            int delta = kExpectedBitrate - bitrate->bps();
            delta = std::abs(delta);
            // Expect the estimation delta to decrease as the window is extended.
            EXPECT_LE(delta, prev_delta + 1);
            prev_delta = delta;
        }
    }
    // Window filled, expect to be close to 8000000.
    EXPECT_EQ(kExpectedBitrate, bitrate->bps());

    now_ms += KWindowSizeMs /* 500 ms */ + 1;
    EXPECT_EQ(0u, stats_.Rate(now_ms)->bps());

    stats_.Update(1000, now_ms);
    ++now_ms;
    stats_.Update(1000, now_ms);
    // We expect two samples of 1000 bytes, and that the bitrate is measured over
    // 500 ms, i.e. 2 * 8 * 1000 / 0.500 = 32000.
    EXPECT_EQ(32000u, stats_.Rate(now_ms)->bps());

    // Reset, add the same samples again.
    stats_.Reset();
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    stats_.Update(1000, now_ms);
    ++now_ms;
    stats_.Update(1000, now_ms);
    // We expect two samples of 1000 bytes, and that the bitrate is measured over
    // 2 ms (window size has been reset) i.e. 2 * 8 * 1000 / 0.002 = 8000000.
    EXPECT_EQ(kExpectedBitrate, stats_.Rate(now_ms)->bps());
}

TEST_F(BitRateStatisticsTest, HandlesChangingWindowSize) {
    int64_t now_ms = 0;
    stats_.Reset();

    // Sanity test window size.
    EXPECT_TRUE(stats_.SetWindowSize(KWindowSizeMs, now_ms));
    EXPECT_FALSE(stats_.SetWindowSize(KWindowSizeMs + 1, now_ms));
    EXPECT_FALSE(stats_.SetWindowSize(0, now_ms));
    EXPECT_TRUE(stats_.SetWindowSize(1, now_ms));
    EXPECT_TRUE(stats_.SetWindowSize(KWindowSizeMs, now_ms));

    // Fill the buffer at a rate of 1 byte / millisecond (8 kbps).
    const int kBatchSize = 10;
    for (int i = 0; i <= KWindowSizeMs; i += kBatchSize)
        stats_.Update(kBatchSize, now_ms += kBatchSize);
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now_ms)->bps());

    // Halve the window size, rate should stay the same.
    EXPECT_TRUE(stats_.SetWindowSize(KWindowSizeMs / 2, now_ms));
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now_ms)->bps());

    // Double the window size again, rate should stay the same. (As the window
    // won't actually expand until new bit and bobs fall into it.
    EXPECT_TRUE(stats_.SetWindowSize(KWindowSizeMs, now_ms));
    EXPECT_EQ(static_cast<uint32_t>(8000), stats_.Rate(now_ms)->bps());

    // Fill the now empty half with bits it twice the rate.
    for (int i = 0; i < KWindowSizeMs / 2; i += kBatchSize)
        stats_.Update(kBatchSize * 2, now_ms += kBatchSize);

    // Rate should have increase be 50%.
    EXPECT_EQ(static_cast<uint32_t>((8000 * 3) / 2), stats_.Rate(now_ms)->bps());
}

TEST_F(BitRateStatisticsTest, RespectsWindowSizeEdges) {
    int64_t now_ms = 0;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now_ms)));

    // One byte per ms, using one big sample.
    stats_.Update(KWindowSizeMs, now_ms);
    now_ms += KWindowSizeMs - 2;
    // Shouldn't work! (Only one sample, not full window size.)
    EXPECT_FALSE(static_cast<bool>(stats_.Rate(now_ms)));

    // Window size should be full, and the single data point should be accepted.
    ++now_ms;
    std::optional<BitRate> bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());

    // Add another, now we have twice the bitrate.
    stats_.Update(KWindowSizeMs, now_ms);
    bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(2 * 1000 * 8u, bitrate->bps());

    // Now that first sample should drop out...
    now_ms += 1;
    bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());
}

TEST_F(BitRateStatisticsTest, HandlesZeroCounts) {
    int64_t now_ms = 0;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    stats_.Update(KWindowSizeMs, now_ms);
    now_ms += KWindowSizeMs - 1;
    stats_.Update(0, now_ms);
    std::optional<BitRate> bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(1000 * 8u, bitrate->bps());

    // Move window along so first data point falls out.
    ++now_ms;
    bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());

    // Move window so last data point falls out.
    now_ms += KWindowSizeMs;
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());
}

TEST_F(BitRateStatisticsTest, HandlesQuietPeriods) {
    int64_t now_ms = 0;
    stats_.Reset();
    // Expecting 0 after init.
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());

    stats_.Update(0, now_ms);
    now_ms += KWindowSizeMs - 1;
    std::optional<BitRate> bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());

    // Move window along so first data point falls out.
    ++now_ms;
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, stats_.Rate(now_ms)->bps());

    // Move window a long way out.
    now_ms += 2 * KWindowSizeMs;
    stats_.Update(0, now_ms);
    bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(0u, bitrate->bps());
}


TEST_F(BitRateStatisticsTest, HandlesBigNumbers) {
    int64_t large_number = 0x100000000u;
    int64_t now_ms = 0;
    stats_.Update(large_number, now_ms++);
    stats_.Update(large_number, now_ms);
    std::optional<BitRate> bitrate = stats_.Rate(now_ms);
    EXPECT_TRUE(bitrate.has_value());
    EXPECT_EQ(large_number * 8000, bitrate->bps());
}

TEST_F(BitRateStatisticsTest, HandlesTooLargeNumbers) {
    int64_t very_large_number = std::numeric_limits<int64_t>::max();
    int64_t now_ms = 0;
    stats_.Update(very_large_number, now_ms++);
    stats_.Update(very_large_number, now_ms);
    // This should overflow the internal accumulator.
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());
}

TEST_F(BitRateStatisticsTest, HandlesSomewhatLargeNumbers) {
    int64_t very_large_number = std::numeric_limits<int64_t>::max();
    int64_t now_ms = 0;
    stats_.Update(very_large_number / 4, now_ms++);
    stats_.Update(very_large_number / 4, now_ms);
    // This should generate a rate of more than int64_t max, but still
    // accumulate less than int64_t overflow.
    EXPECT_FALSE(stats_.Rate(now_ms).has_value());
}
    
} // namespace test    
} // namespace naivertc
