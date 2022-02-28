#include "rtc/congestion_controller/goog_cc/delay_based/aimd_rate_control.hpp"
#include "testing/simulated_clock.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr int64_t kClockInitialTime = 123456;

constexpr int kMinBwePeriodMs = 2000; // 2s
constexpr int kDefaultPeriodMs = 3000; // 3s
constexpr int kMaxBwePeriodMs = 50000; // 50s

// After an overuse, we back off to 85% to the received bitrate.
constexpr double kFractionAfterOveruse = 0.85;

struct AimdRateControlStates {
    std::unique_ptr<AimdRateControl> aimd_rate_control;
    std::unique_ptr<SimulatedClock> simulated_clock;
};

AimdRateControlStates CreateAimdRateControlStates(bool send_side = false, bool no_bitrate_increase_in_alr = false) {
    AimdRateControlStates states;
    auto config = AimdRateControl::Configuration();
    config.no_bitrate_increase_in_alr = no_bitrate_increase_in_alr;
    states.aimd_rate_control.reset(new AimdRateControl(std::move(config), send_side));
    states.simulated_clock.reset(new SimulatedClock(kClockInitialTime));
    return states;
}

std::optional<DataRate> ToDataRate(std::optional<int> bitrate_bps) {
    if (bitrate_bps) {
        return DataRate::BitsPerSec(*bitrate_bps);
    } else {
        return std::nullopt;
    }
}

void UpdateRateControl(const AimdRateControlStates& states, 
                       const BandwidthUsage& bw_usage, 
                       std::optional<int> throughput_estimate,
                       int64_t now_ms) {
    states.aimd_rate_control->Update(bw_usage, ToDataRate(throughput_estimate), Timestamp::Millis(now_ms));
}

void SetEstimate(const AimdRateControlStates& states, int bitrate_bps) {
    states.aimd_rate_control->SetEstimate(DataRate::BitsPerSec(bitrate_bps), states.simulated_clock->CurrentTime());
}
    
} // namespace

MY_TEST(AimdRateControlTest, MinNearMaxIncreaseRateOnLowBandwidth) {
    auto states = CreateAimdRateControlStates();
    constexpr int kBitrateBps = 30000; // 30kbps
    SetEstimate(states, kBitrateBps);
    EXPECT_EQ(4000, states.aimd_rate_control->GetNearMaxIncreaseRatePerSecond().bps());
}

MY_TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn90kbpsAnd200msRtt) {
    auto states = CreateAimdRateControlStates();
    constexpr int kBitrateBps = 90000; // 90kbps
    SetEstimate(states, kBitrateBps);
    EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRatePerSecond().bps());
}

MY_TEST(AimdRateControlTest, NearMaxIncreaseRateIs5kbpsOn90kbpsAnd100msRtt) {
    auto states = CreateAimdRateControlStates();
    constexpr int kBitrateBps = 60000; // 60kbps
    SetEstimate(states, kBitrateBps);
    states.aimd_rate_control->set_rtt(TimeDelta::Millis(100));
    EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRatePerSecond().bps());
}

MY_TEST(AimdRateControlTest, GetIncreaseRateAndBandwidthPeriod) {
    auto states = CreateAimdRateControlStates();
    constexpr int kBitrateBps = 300000; // 300kbps
    SetEstimate(states, kBitrateBps);
    UpdateRateControl(states, BandwidthUsage::OVERUSING, kBitrateBps, states.simulated_clock->now_ms());
    EXPECT_NEAR(14000, states.aimd_rate_control->GetNearMaxIncreaseRatePerSecond().bps(), 1000);
    EXPECT_EQ(kDefaultPeriodMs, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
}

MY_TEST(AimdRateControlTest, BweLimitedByAckedBitrate) {
    auto states = CreateAimdRateControlStates();
    constexpr int kAckedBitrateBps = 10000; // 10kbps
    constexpr double throughput_based_limit = 1.5 * kAckedBitrateBps + 10000 /* 10kbps*/;
    SetEstimate(states, kAckedBitrateBps);
    while (states.simulated_clock->now_ms() - kClockInitialTime < 20000 /* 20s */) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, kAckedBitrateBps, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    ASSERT_TRUE(states.aimd_rate_control->ValidEstimate());
    EXPECT_EQ(static_cast<uint32_t>(throughput_based_limit), states.aimd_rate_control->LatestEstimate().bps());
}

MY_TEST(AimdRateControlTest, BweNotLimitedByDecreaseingAckedBitrate) {
    auto states = CreateAimdRateControlStates();
    constexpr int kAckedBitrateBps = 100000; // 100kbps
    constexpr double throughput_based_limit = 1.5 * kAckedBitrateBps + 10000 /* 10kbps*/;
    SetEstimate(states, kAckedBitrateBps);
    while (states.simulated_clock->now_ms() - kClockInitialTime < 20000 /* 20s */) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, kAckedBitrateBps, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    ASSERT_TRUE(states.aimd_rate_control->ValidEstimate());
    // If the acked bitrate decrease the BWE shouldn't be reduced to 1.5x
    // what's being acked, but also shouldn't get to increase more.
    uint32_t prev_estimate = states.aimd_rate_control->LatestEstimate().bps();
    UpdateRateControl(states, BandwidthUsage::NORMAL, kAckedBitrateBps / 2, states.simulated_clock->now_ms());
    uint32_t new_estimate = states.aimd_rate_control->LatestEstimate().bps();
    EXPECT_NEAR(new_estimate, static_cast<uint32_t>(throughput_based_limit), 2000);
    EXPECT_EQ(new_estimate, prev_estimate);
}

MY_TEST(AimdRateControlTest, DefaultPeriodUntilFirstOveruse) {
    auto states = CreateAimdRateControlStates();
    states.aimd_rate_control->SetStartBitrate(DataRate::KilobitsPerSec(300));
    EXPECT_EQ(kDefaultPeriodMs, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
    states.simulated_clock->AdvanceTimeMs(100);
    UpdateRateControl(states, BandwidthUsage::OVERUSING, 280000, states.simulated_clock->now_ms());
    EXPECT_NE(kDefaultPeriodMs, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
}

MY_TEST(AimdRateControlTest, ExpectedPeriodAfter20kbpsDropAnd5kbpsIncrease) {
    auto states = CreateAimdRateControlStates();
    constexpr int kInitialBitrateBps = 110000;
    SetEstimate(states, kInitialBitrateBps);
    // Make the bitrate drop by 20 kbps to get to 90 kbps.
    // The rate increase at 90 kbps should be 5 kbps, so the 
    // period should be 4 s.
    constexpr int kAckedBitrateBps = (kInitialBitrateBps - 20000) / kFractionAfterOveruse;
    UpdateRateControl(states, BandwidthUsage::OVERUSING, kAckedBitrateBps, states.simulated_clock->now_ms());

    EXPECT_EQ(5000, states.aimd_rate_control->GetNearMaxIncreaseRatePerSecond().bps());
    EXPECT_EQ(4000, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
}

MY_TEST(AimdRateControlTest, BandwidthPeriodIsNotBelowMin) {
    auto states = CreateAimdRateControlStates();
    constexpr int kInitialBitrateBps = 10000; // 10 kbps
    SetEstimate(states, kInitialBitrateBps);
    states.simulated_clock->AdvanceTimeMs(100);
    // Make a small (1.5 kbps) bitrate drop to 8.5 kbps.
    UpdateRateControl(states, BandwidthUsage::OVERUSING, kInitialBitrateBps - 1, states.simulated_clock->now_ms());
    EXPECT_EQ(kMinBwePeriodMs, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
}

MY_TEST(AimdRateControlTest, BandwidthPeriodIsNotAboveMaxNoSmoothingExp) {
    auto states = CreateAimdRateControlStates();
    constexpr int kInitialBitrateBps = 10010000; // 10010 kbps
    SetEstimate(states, kInitialBitrateBps);
    states.simulated_clock->AdvanceTimeMs(100);
    // Make a large (10 Mbps) bitrate drop 10 kbps.
    constexpr int kAckedBitrateBps = 10000 / kFractionAfterOveruse;
    UpdateRateControl(states, BandwidthUsage::OVERUSING, kAckedBitrateBps, states.simulated_clock->now_ms());
    EXPECT_EQ(kMaxBwePeriodMs, states.aimd_rate_control->GetExpectedBandwidthPeriod().ms());
}

MY_TEST(AimdRateControlTest, SendingRateBoundedWhenThroughputNotEstimated) {
    auto states = CreateAimdRateControlStates();
    constexpr int kInitialBitrateBps = 123000; // 123 kbps
    constexpr double throughput_based_limit = 1.5 * kInitialBitrateBps + 10000 /* 10kbps*/;
    UpdateRateControl(states, BandwidthUsage::NORMAL, kInitialBitrateBps, states.simulated_clock->now_ms());
    // AimdRateControl sets the initial bit rate to what it receives after
    // five seconds has passed.
    constexpr int kInitializationTimeMs = 5000;
    states.simulated_clock->AdvanceTimeMs(kInitializationTimeMs + 1);
    UpdateRateControl(states, BandwidthUsage::NORMAL, kInitialBitrateBps, states.simulated_clock->now_ms());
    for (int i = 0; i < 100; ++i) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, std::nullopt, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    EXPECT_LE(states.aimd_rate_control->LatestEstimate().bps(), throughput_based_limit);
}

MY_TEST(AimdRateControlTest, EstimateDoesNotIncreaseInAlr) {
    auto states = CreateAimdRateControlStates(/*send_side=*/true, /*no_bitrate_increase_in_alr=*/true);
    constexpr int kInitialBitrateBps = 123000; // 123 kbps
    SetEstimate(states, kInitialBitrateBps);
    states.aimd_rate_control->set_in_alr(true);
    for (int i = 0; i < 100; ++i) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, std::nullopt, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    EXPECT_EQ(states.aimd_rate_control->LatestEstimate().bps(), kInitialBitrateBps);

    SetEstimate(states, 2 * kInitialBitrateBps);
    for (int i = 0; i < 100; ++i) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, std::nullopt, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    EXPECT_EQ(states.aimd_rate_control->LatestEstimate().bps(), 2 * kInitialBitrateBps);
}

MY_TEST(AimdRateControlTest, EstimateIncreaseWhileNotInAlr) {
    // Allow the estimate to increase as long as alr is not detected to 
    // ensure the BWE can not get stuck at a certain bitrate.
    auto states = CreateAimdRateControlStates(/*send_side=*/true, /*no_bitrate_increase_in_alr=*/true);
    constexpr int kInitialBitrateBps = 123000; // 123 kbps
    SetEstimate(states, kInitialBitrateBps);
    states.aimd_rate_control->set_in_alr(false);
    UpdateRateControl(states, BandwidthUsage::NORMAL, kInitialBitrateBps, states.simulated_clock->now_ms());
    for (int i = 0; i < 100; ++i) {
        UpdateRateControl(states, BandwidthUsage::NORMAL, std::nullopt, states.simulated_clock->now_ms());
        states.simulated_clock->AdvanceTimeMs(100);
    }
    EXPECT_GT(states.aimd_rate_control->LatestEstimate().bps(), kInitialBitrateBps);

}

} // namespace test
} // namespace naivertc