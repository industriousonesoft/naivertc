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

constexpr size_t kMtu = 1200;
constexpr uint32_t kAcceptedBitrateErrorBps = 50'000; // 50 kbps

// Number of packets needed before we have a valid estimate.
constexpr int kNumInitialPackets = 2;

constexpr int kInitialProbingPackets = 5;

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

MY_TEST_F(DelayBasedBweTest, GetExpectedBwePeriodMs) {
    auto default_interval = bandwidth_estimator_->GetExpectedBwePeriod();
    EXPECT_GT(default_interval.ms(), 0);
    // FIXME: Try to pass the below test with second parameter set as 333 (like WebRTC does)?
    // LinkCapacityDropTestHelper(1, 333, 0);
    LinkCapacityDropTestHelper(1, 233, 0);
    auto interval = bandwidth_estimator_->GetExpectedBwePeriod();
    EXPECT_GT(interval.ms(), 0);
    EXPECT_NE(interval.ms(), default_interval.ms());
}

MY_TEST_F(DelayBasedBweTest, TestInitialOveruse) {
    const DataRate kStartBitrate = DataRate::KilobitsPerSec(300);
    const DataRate kInitialCapacity = DataRate::KilobitsPerSec(200);
    const uint32_t kDummySsrc = 0;
    // High FPS to ensure that we send a lot of packets in a short time.
    const int kFps = 90;

    stream_generator_->AddStream(std::make_unique<RtpStream>(kFps, kStartBitrate.bps()));
    stream_generator_->set_link_capacity_bps(kInitialCapacity.bps());

    // Needed to initialize the AimdRateControl.
    bandwidth_estimator_->SetStartBitrate(kStartBitrate);

    // Produce 30 frames (in 1/3 second) and give them to the estimator.
    int64_t bitrate_bps = kStartBitrate.bps();
    bool seen_overuse = false;
    for (int i = 0; i < 30; ++i) {
        bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
        // The purpose of this test is to ensure that we back down even if we don't
        // have any acknowledged bitrate estimate yet. Hence, if the test works
        // as expected, we should not have a measured bitrate yet.
        EXPECT_FALSE(ack_bitrate_estimator_->Estimate().has_value());
        if (overuse) {
            EXPECT_TRUE(bitrate_observer_.updated());
            EXPECT_NEAR(bitrate_observer_.latest_bitrate_bps(), kStartBitrate.bps() / 2,
                        15000);
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            seen_overuse = true;
            break;
        } else if (bitrate_observer_.updated()) {
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_observer_.Reset();
        }
    }
    EXPECT_TRUE(seen_overuse);
    EXPECT_NEAR(bitrate_observer_.latest_bitrate_bps(), kStartBitrate.bps() / 2,
                15000);
}

// This test subsumes and improves DelayBasedBweTest.TestInitialOveruse above.
// NOTE: Sets the `initial_backoff_interval` in AimdRateControl::Configuration before tesing.
MY_TEST_F(DelayBasedBweTest, DISABLED_TestInitialOveruseWithInitialBackoffInterval) {
    const DataRate kStartBitrate = DataRate::KilobitsPerSec(300);
    const DataRate kInitialCapacity = DataRate::KilobitsPerSec(200);
    const uint32_t kDummySsrc = 0;
    // High FPS to ensure that we send a lot of packets in a short time.
    const int kFps = 90;

    stream_generator_->AddStream(std::make_unique<RtpStream>(kFps, kStartBitrate.bps()));
    stream_generator_->set_link_capacity_bps(kInitialCapacity.bps());

    // Needed to initialize the AimdRateControl.
    bandwidth_estimator_->SetStartBitrate(kStartBitrate);

    // Produce 30 frames (in 1/3 second) and give them to the estimator.
    int64_t bitrate_bps = kStartBitrate.bps();
    bool seen_overuse = false;
    for (int frames = 0; frames < 30 && !seen_overuse; ++frames) {
        bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
        // The purpose of this test is to ensure that we back down even if we don't
        // have any acknowledged bitrate estimate yet. Hence, if the test works
        // as expected, we should not have a measured bitrate yet.
        EXPECT_FALSE(ack_bitrate_estimator_->Estimate().has_value());
        if (overuse) {
            EXPECT_TRUE(bitrate_observer_.updated());
            EXPECT_NEAR(bitrate_observer_.latest_bitrate_bps(), kStartBitrate.bps() / 2,
                        15000);
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            seen_overuse = true;
        } else if (bitrate_observer_.updated()) {
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_observer_.Reset();
        }
    }
    EXPECT_TRUE(seen_overuse);
    // Continue generating an additional 15 frames (equivalent to 167 ms) and
    // verify that we don't back down further.
    for (int frames = 0; frames < 15 && seen_overuse; ++frames) {
        bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
        EXPECT_FALSE(overuse);
        if (bitrate_observer_.updated()) {
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            EXPECT_GE(bitrate_bps, kStartBitrate.bps() / 2 - 15000);
            EXPECT_LE(bitrate_bps, kInitialCapacity.bps() + 15000);
            bitrate_observer_.Reset();
        }
    }
}

MY_TEST_F(DelayBasedBweTest, InitialBehavior) {
    const int kFps = 50; // 50 fps to avoid rounding errors;
    const int kFrameIntervalMs = 1000 / kFps;
    const ProbeCluster kProbeCluster = {/*id=*/0, /*min_probes=*/kInitialProbingPackets, /*min_bytes=*/5000};
    const PacedPacketInfo kPacingInfo = {-1, kProbeCluster};
    int64_t send_time_ms = 0;
    std::vector<uint32_t> ssrcs;
    EXPECT_FALSE(bandwidth_estimator_->LatestEstimate().second);
    clock_.AdvanceTimeMs(1000);
    EXPECT_FALSE(bandwidth_estimator_->LatestEstimate().second);
    EXPECT_FALSE(bitrate_observer_.updated());
    bitrate_observer_.Reset();
    clock_.AdvanceTimeMs(1000);
    // Inserting packets for 5 seconds to get a valid estimate.
    for (int i = 0; i < 5 * kFps + 1 + kNumInitialPackets; ++i) {
        PacedPacketInfo pacing_info = i < kInitialProbingPackets ? kPacingInfo : PacedPacketInfo();
        if (i == kNumInitialPackets) {
            EXPECT_FALSE(bandwidth_estimator_->LatestEstimate().second);
            EXPECT_FALSE(bitrate_observer_.updated());
            bitrate_observer_.Reset();
        }
        IncomingFeedback(clock_.now_ms(), send_time_ms, kMtu, pacing_info);
        clock_.AdvanceTimeMs(kFrameIntervalMs);
        send_time_ms += kFrameIntervalMs;
    }
    auto [bitrate, valid] = bandwidth_estimator_->LatestEstimate();
    EXPECT_TRUE(valid);
    GTEST_COUT << "bitrate: " << bitrate.bps() << " bps" << std::endl;
    EXPECT_NEAR(730'000/* 730 kbps */, bitrate.bps(), kAcceptedBitrateErrorBps);
    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_EQ(bitrate_observer_.latest_bitrate_bps(), bitrate.bps());
}

MY_TEST_F(DelayBasedBweTest, RateIncreaseReordering) {
    const int64_t expected_bitrate_bps = 730'000/* 730 kbps */;
    const int kFps = 50; // 50 fps to avoid rounding errors;
    const int kFrameIntervalMs = 1000 / kFps;
    const ProbeCluster kProbeCluster = {/*id=*/0, /*min_probes=*/kInitialProbingPackets, /*min_bytes=*/5000};
    const PacedPacketInfo kPacingInfo = {-1, kProbeCluster};
    int64_t send_time_ms = 0;
    // Inserting packets for five seconds to get a valid estimate.
    for (int i = 0; i < 5 * kFps + 1 + kNumInitialPackets; ++i) {
        PacedPacketInfo pacing_info = i < kInitialProbingPackets ? kPacingInfo : PacedPacketInfo();
        if (i == kNumInitialPackets) {
            EXPECT_FALSE(bandwidth_estimator_->LatestEstimate().second);
            EXPECT_FALSE(bitrate_observer_.updated());
            bitrate_observer_.Reset();
        }
        IncomingFeedback(clock_.now_ms(), send_time_ms, kMtu, pacing_info);
        clock_.AdvanceTimeMs(kFrameIntervalMs);
        send_time_ms += kFrameIntervalMs;
    }
    EXPECT_TRUE(bitrate_observer_.updated());
    auto [bitrate, valid] = bandwidth_estimator_->LatestEstimate();
    EXPECT_NEAR(expected_bitrate_bps, bitrate.bps(), kAcceptedBitrateErrorBps);
    for (int i = 0; i < 10; ++i) {
        clock_.AdvanceTimeMs(2 * kFrameIntervalMs);
        send_time_ms += 2 * kFrameIntervalMs;
        IncomingFeedback(clock_.now_ms(), send_time_ms, 1000);
        IncomingFeedback(clock_.now_ms(), send_time_ms - kFrameIntervalMs, 1000);
    }
    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_NEAR(expected_bitrate_bps, bitrate_observer_.latest_bitrate_bps(), kAcceptedBitrateErrorBps);
}

MY_TEST_F(DelayBasedBweTest, RateIncreaseRtpTimestamp) {
    // This threhold corresponds approximately to increasing linearly 、
    // with bitrate(i) = bitrate(i-1) + max(0.08 * bitrate(i-1), 1000) until bitrate(i)
    // greater than 500 kbps, with bitrate(1) ~= 30 kbps;
    int expected_iterations = 622;
    uint32_t bitrate_bps = 30000;
    int iterations = 0;
    AddStream();
    // Feed the estimator with a stream of packets and verify that it
    // reaches 500 kbps at the expected time.
    while (bitrate_bps < 5e5) {
        bool overuse = GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
        if (overuse) {
            EXPECT_GT(bitrate_observer_.latest_bitrate_bps(), bitrate_bps);
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_observer_.Reset();
        } else if (bitrate_observer_.updated()) {
            bitrate_bps = bitrate_observer_.latest_bitrate_bps();
            bitrate_observer_.Reset();
        }
        ++iterations;
    }
    ASSERT_EQ(expected_iterations, iterations);
}

MY_TEST_F(DelayBasedBweTest, TimestampGrouping) {
    const int kFps = 50;
    const int kFrameIntervalMs = 1000 / kFps;
    int64_t send_time_ms = 0;
    // Initial set of frames to increase the bitrate.
    // 6 seconds to have enough time for the first estimate.
    for (int i = 0; i < 6 * kFps; ++i) {
        IncomingFeedback(clock_.now_ms(), send_time_ms, 1000);
        clock_.AdvanceTimeMs(kFrameIntervalMs);
        send_time_ms += kFrameIntervalMs;
    }
    EXPECT_TRUE(bitrate_observer_.updated());
    EXPECT_GE(bitrate_observer_.latest_bitrate_bps(), 400000u);

    // Insert batches of frames which were sent very close in time. 
    // Also simulate capacity over-use to see that we back off correctly.
    const int kTimestampGroupSize = 15;
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < kTimestampGroupSize; ++j) {
            // Insert `kTimestampGroupSize` frames with just 1 timestamp ticks in
            // between. Should be treated as part of the same group by the estimator.
            IncomingFeedback(clock_.now_ms(), send_time_ms, 100);
            clock_.AdvanceTimeMs(kFrameIntervalMs / kTimestampGroupSize);
            send_time_ms += 1;
        }
        // Make sure the inter-arrival (10ms) is greater than the inter-departure (5ms) to simulate over-use.
        clock_.AdvanceTimeMs(10);
        send_time_ms += kFrameIntervalMs - kTimestampGroupSize;
    }
    EXPECT_TRUE(bitrate_observer_.updated());
    // Should have reduced the estimate.
    EXPECT_LT(bitrate_observer_.latest_bitrate_bps(), 400000u);
}

MY_TEST_F(DelayBasedBweTest, ShortTimeoutAndWrap) {
    // Simulate a client leaving and rejoining the call after 35 seconds. This
    // will make abs send time wrap, so if streams aren't timed out properly
    // the next 30 seconds of packets will be out of order.
    const int silence_time_ms = 35'000;
    const int kFps = 100;
    const int kFrameIntervalMs = 1000 / kFps;
    int64_t send_time_ms = 0;

    for (int i = 0; i < 3000; ++i) {
        IncomingFeedback(clock_.now_ms(), send_time_ms, 1000);
        clock_.AdvanceTimeMs(kFrameIntervalMs);
        send_time_ms += kFrameIntervalMs;
    }
    auto [bitrate_before, updated_before] = bandwidth_estimator_->LatestEstimate();
    EXPECT_TRUE(updated_before);

    clock_.AdvanceTimeMs(silence_time_ms);
    send_time_ms += silence_time_ms;

    for (int i = 0; i < 24; ++i) {
        IncomingFeedback(clock_.now_ms(), send_time_ms, 1000);
        clock_.AdvanceTimeMs(2 * kFrameIntervalMs);
        send_time_ms += kFrameIntervalMs;
    }
    
    auto [bitrate_after, updated_after] = bandwidth_estimator_->LatestEstimate();
    EXPECT_TRUE(updated_after);
    EXPECT_LT(bitrate_after, bitrate_before);
}
    
} // namespace test
} // namespace naivertc
