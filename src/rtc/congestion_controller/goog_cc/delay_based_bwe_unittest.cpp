#include "rtc/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.hpp"
#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {
namespace test {
namespace {
constexpr int kNumProbesCluster0 = 5;
constexpr int kNumProbesCluster1 = 8;
const ProbeCluster kProbeCluster0 = {0, kNumProbesCluster0, 2000};
const ProbeCluster kProbeCluster1 = {1, kNumProbesCluster1, 4000};
const PacedPacketInfo kPacingInfo0 = {-1, kProbeCluster0};
const PacedPacketInfo kPacingInfo1 = {-1, kProbeCluster1};
constexpr float kTargetUtilizationFraction = 0.95f;
} // namespace

MY_TEST_F(DelayBasedBweTest, ProbeDetection) {
    int64_t now_ms = clock_.now_ms();

    // NOTE: the probed bitrate works and the ack bitrate is not triggered yet
    // since it's initial window is 500 ms.

    // First burst sent at 8 * 1000 / 10 = 800 kbps.
    for (int i = 0; i < kNumProbesCluster0; ++i) {
        clock_.AdvanceTimeMs(10);
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo0);
    }
    EXPECT_TRUE(bitrate_observer_.updated());
    // The returned bitrate is set slightly lower than (5% off) the probed bitrate.
    EXPECT_GT(bitrate_observer_.latest_bitrate_bps(), 700'000);

    // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
    for (int i = 0; i < kNumProbesCluster1; ++i) {
        clock_.AdvanceTimeMs(5);
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo1);
    }

    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_GT(bitrate_observer_.latest_bitrate_bps(), 1500'000u);

}

MY_TEST_F(DelayBasedBweTest, ProbeDetectionNonPacedPackets) {
    int64_t now_ms = clock_.now_ms();
    // First burst sent at 8 * 1000 / 10 = 800 kbps,
    // but with every other packet not being paced
    // which could mess things up.
    for (int i = 0; i < kNumProbesCluster0; ++i) {
        clock_.AdvanceTimeMs(5);
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo0);
        // Non-paced packet, arriving 5 ms after.
        clock_.AdvanceTimeMs(5);
        IncomingFeedback(now_ms, now_ms, 100);
    }

    EXPECT_TRUE(bitrate_observer_.updated());
    // This will return the maximum bitrate (30000 kbps) set in AimdRateControl,
    // since we are not reach the initial window (500 ms) to estimate the bitrate yet.
    EXPECT_GT(bitrate_observer_.latest_bitrate_bps(), 800000u);
}

MY_TEST_F(DelayBasedBweTest, ProbeDetectionFasterArrival) {
    int64_t now_ms = clock_.now_ms();
    // First burst sent at 8 * 1000 / 10 = 800 kbps.
    // Arriving at 8 * 1000 / 5 = 1600 kbps.
    int64_t send_time_ms = 0;
    for (int i = 0; i < kNumProbesCluster0; ++i) {
        clock_.AdvanceTimeMs(1);
        send_time_ms += 10;
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo0);
    }
    EXPECT_FALSE(bitrate_observer_.updated());
}

MY_TEST_F(DelayBasedBweTest, ProbeDetectionSlowerArrival) {
    int64_t now_ms = clock_.now_ms();
    // First burst sent at 8 * 1000 / 5 = 1600 kbps.
    // Arriving at 8 * 1000 / 7 = 1142 kbps.
    // Since the receive rate is significantly below the send rate, we expect to
    // use 95% of the estimated capacity.
    int64_t send_time_ms = 0;
    for (int i = 0; i < kNumProbesCluster1; ++i) {
        clock_.AdvanceTimeMs(7);
        send_time_ms += 5;
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo1);
    }

    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_NEAR(bitrate_observer_.latest_bitrate_bps(),
                kTargetUtilizationFraction * 1140000u, 10000u);
}

MY_TEST_F(DelayBasedBweTest, ProbeDetectionSlowerArrivalHighBitrate) {
    int64_t now_ms = clock_.now_ms();
    // Burst sent at 8 * 1000 / 1 = 8000 kbps.
    // Arriving at 8 * 1000 / 2 = 4000 kbps.
    // Since the receive rate is significantly below the send rate, we expect to
    // use 95% of the estimated capacity.
    int64_t send_time_ms = 0;
    for (int i = 0; i < kNumProbesCluster1; ++i) {
        clock_.AdvanceTimeMs(2);
        send_time_ms += 1;
        now_ms = clock_.now_ms();
        IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo1);
    }

    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_NEAR(bitrate_observer_.latest_bitrate_bps(),
                kTargetUtilizationFraction * 4000000u, 10000u);
}
    
} // namespace test
} // namespace naivertc
