#include "rtc/congestion_control/controllers/goog_cc/probe/probe_controller.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

#include <vector>

namespace naivertc {
namespace test {
namespace {
    
constexpr DataRate kMinBitrate = DataRate::BitsPerSec(100);
constexpr DataRate kStartBitrate = DataRate::BitsPerSec(300);
constexpr DataRate kMaxBitrate = DataRate::BitsPerSec(10'000);

constexpr int kExponentialProbingTimeoutMs = 5000;

constexpr int kAlrProbeIntervalMs = 5000;
constexpr int kAlrEndedTimeoutMs = 3000;
constexpr int kBitrateDropTimeoutMs = 5000;

} // namespace

class T(ProbeControllerTest) : public ::testing::Test {
protected:
    T(ProbeControllerTest)() : clock_(1000'000) {
        ProbeController::Configuration config;
        probe_ctrl_.reset(new ProbeController(config));
    }
    ~T(ProbeControllerTest)() override {}

    Timestamp Now() {
        return clock_.CurrentTime();
    }

protected:
    SimulatedClock clock_;
    std::unique_ptr<ProbeController> probe_ctrl_;
};

MY_TEST_F(ProbeControllerTest, InitProbingStart) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_GE(probes.size(), 2);
}

MY_TEST_F(ProbeControllerTest, MidCallProbingOnMaxBitrateIncrease) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());

    DataRate new_max_bitrate = kMaxBitrate + DataRate::BitsPerSec(100);
    // Long enough to time out exponential probing.
    clock_.AdvanceTimeMs(kExponentialProbingTimeoutMs);
    probes = probe_ctrl_->OnEstimatedBitrate(kStartBitrate, Now());
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    ASSERT_TRUE(probes.empty());
    // Trigger mid call probing to |new_max_bitrate|.
    probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, new_max_bitrate, Now());

    ASSERT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, new_max_bitrate);
}

MY_TEST_F(ProbeControllerTest, ProbesOnMaxBitrateIncreaseOnlyWhenInAlr) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());

    probes = probe_ctrl_->OnEstimatedBitrate(kMaxBitrate - DataRate::BitsPerSec(1), Now());

    // Wait long enough to time out exponential probing.
    clock_.AdvanceTimeMs(kExponentialProbingTimeoutMs);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_TRUE(probes.empty());

    // Probe when in ALR.
    probe_ctrl_->set_alr_start_time(Now());
    probes = probe_ctrl_->OnMaxTotalAllocatedBitrate(kMaxBitrate + DataRate::BitsPerSec(1), Now());
    EXPECT_EQ(probes.size(), 2);

    // Dont probe when not in ALR.
    probe_ctrl_->set_alr_start_time(std::nullopt);
    probes = probe_ctrl_->OnMaxTotalAllocatedBitrate(kMaxBitrate + DataRate::BitsPerSec(2), Now());
    EXPECT_TRUE(probes.empty());

}

MY_TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncreaseAtMaxBitrate) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    // Long enough to time out exponential probing.
    clock_.AdvanceTimeMs(kExponentialProbingTimeoutMs);
    probes = probe_ctrl_->OnEstimatedBitrate(kStartBitrate, Now());
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    probes = probe_ctrl_->OnEstimatedBitrate(kMaxBitrate, Now());
    // Trigger mid call probing on max birates increased
    probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate + DataRate::BitsPerSec(100), Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, kMaxBitrate + DataRate::BitsPerSec(100));
}


MY_TEST_F(ProbeControllerTest, TestExponentialProbing) {
  auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());

  // Repeated probe should only be sent when estimated bitrate climbs above
  // 0.7 * 6 * kStartBitrate = 1260.
  probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(1000), Now());
  EXPECT_EQ(probes.size(), 0u);

  probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(1800), Now());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_bitrate, DataRate::BitsPerSec(2 * 1800));
}

MY_TEST_F(ProbeControllerTest, TestExponentialProbingTimeout) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    // Advance far enough to cause a time out in waiting for probing result.
    clock_.AdvanceTimeMs(kExponentialProbingTimeoutMs);
    // Cancel the further probe when time out.
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // No futher pro
    probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(1800), Now());
    EXPECT_EQ(probes.size(), 0u);
}

MY_TEST_F(ProbeControllerTest, RequestProbeInAlr) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_GE(probes.size(), 2u);
    DataRate estimated_bitrate = DataRate::BitsPerSec(500);
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    probe_ctrl_->set_alr_start_time(Now());
    clock_.AdvanceTimeMs(kAlrProbeIntervalMs + 1);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // A large drop happens: 500 -> 250, drop 50% < 66% (drop detect threshold).
    probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(250), Now());
    // Request a probe after droping.
    probes = probe_ctrl_->RequestProbe(Now());

    EXPECT_EQ(probes.size(), 1u);
    // Last estimate before droping * 0.85
    EXPECT_EQ(probes[0].target_bitrate, estimated_bitrate * 0.85);
}

MY_TEST_F(ProbeControllerTest, RequestProbeWhenAlrEndedRecently) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_EQ(probes.size(), 2u);
    DataRate estimated_bitrate = DataRate::BitsPerSec(500);
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    // No in ALR state but just ended recently.
    probe_ctrl_->set_alr_start_time(std::nullopt);
    clock_.AdvanceTimeMs(kAlrProbeIntervalMs + 1);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // A large drop happens.
    probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(250), Now());
    probe_ctrl_->set_alr_end_time(Now());
    clock_.AdvanceTimeMs(kAlrEndedTimeoutMs - 1);
    // Request probe when ALR ended recently.
    probes = probe_ctrl_->RequestProbe(Now());

    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, estimated_bitrate * 0.85);
}

MY_TEST_F(ProbeControllerTest, RequestProbeWhenAlrNotEndedRecently) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_EQ(probes.size(), 2u);
    DataRate estimated_bitrate = DataRate::BitsPerSec(500);
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    // No in ALR state but just ended recently.
    probe_ctrl_->set_alr_start_time(std::nullopt);
    clock_.AdvanceTimeMs(kAlrProbeIntervalMs + 1);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // A large drop happens.
    probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(250), Now());
    probe_ctrl_->set_alr_end_time(Now());
    // ALR ended time out.
    clock_.AdvanceTimeMs(kAlrEndedTimeoutMs + 1);
    probes = probe_ctrl_->RequestProbe(Now());

    EXPECT_TRUE(probes.empty());
}

MY_TEST_F(ProbeControllerTest, RequestProbeWhenBweDropNotRecent) {
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_EQ(probes.size(), 2u);
    DataRate estimated_bitrate = DataRate::BitsPerSec(500);
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    // in ALR state
    probe_ctrl_->set_alr_start_time(Now());
    clock_.AdvanceTimeMs(kAlrProbeIntervalMs + 1);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // A large drop happens.
    probes = probe_ctrl_->OnEstimatedBitrate(DataRate::BitsPerSec(250), Now());
    // Advance far enough to cause the last drop request time out.
    clock_.AdvanceTimeMs(kBitrateDropTimeoutMs + 1);
    probes = probe_ctrl_->RequestProbe(Now());

    EXPECT_TRUE(probes.empty());
}

MY_TEST_F(ProbeControllerTest, PeriodicProbing) {
    probe_ctrl_->set_enable_periodic_alr_probing(true);
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_EQ(probes.size(), 2u);
    DataRate estimated_bitrate = DataRate::BitsPerSec(500);
    // Repeated probe should only be sent when estimated bitrate climbs above
    // 0.7 * 6 * kStartBitrate = 1260.
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());
    EXPECT_TRUE(probes.empty());

    auto start_time = Now();

    // Expect the controller to send a new probe after 5s has passed.
    probe_ctrl_->set_alr_start_time(start_time);
    // A alr_probing_interval has passed.
    clock_.AdvanceTimeMs(5000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 1u);
    // Last estimate * alr_probe_scale = 500 * 2 = 1000
    EXPECT_EQ(probes[0].target_bitrate, DataRate::BitsPerSec(1000));
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    // Still in alr_probing_interval
    clock_.AdvanceTimeMs(4000);
    // No probe will be sent.
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 0u);
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());

    // A alr_probing_interval has passed.
    clock_.AdvanceTimeMs(1000);
    // Expect the controller to send a new probe.
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 1u);
    // No further probe.
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());
    EXPECT_EQ(probes.size(), 0u);
}

MY_TEST_F(ProbeControllerTest, PeriodicProbingAfterReset) {
    probe_ctrl_->set_alr_start_time(Now());
    probe_ctrl_->set_enable_periodic_alr_probing(true);
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    probe_ctrl_->Reset(Now());

    clock_.AdvanceTimeMs(10000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    // Since bitrates are not yet set, no probe is sent event though we are in ALR
    // mode.
    EXPECT_EQ(probes.size(), 0u);

    probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, kMaxBitrate, Now());
    EXPECT_EQ(probes.size(), 2u);

    // Make sure we use |kStartBitrate| as the estimated bitrate
    // until OnEstimatedBitrate is called with an updated estimate.
    clock_.AdvanceTimeMs(10000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, kStartBitrate * 2);
}

MY_TEST_F(ProbeControllerTest, TestExponentialProbingOverflow) {
    const auto kMultiplier = DataRate::BitsPerSec(1000'000);
    const auto start_birate = 10 * kMultiplier;
    const auto max_birate = 100 * kMultiplier;
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, start_birate, max_birate, Now());
    
    // Repeated probe will be sent when estimated bitrate climbs above
    // 0.7 * 6 * start_birate = 42 * kMultiplier.
    auto estimated = 60 * kMultiplier;
    probes = probe_ctrl_->OnEstimatedBitrate(estimated, Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, max_birate);
    // Verify that probe bitrate is capped at the specified max bitrate.
    probes = probe_ctrl_->OnEstimatedBitrate(max_birate, Now());
    EXPECT_EQ(probes.size(), 0u);
}

MY_TEST_F(ProbeControllerTest, TestAllocatedBitrateCap) {
    const auto kMultiplier = DataRate::BitsPerSec(1000'000);
    const auto start_birate = 10 * kMultiplier;
    const auto max_birate = 100 * kMultiplier;
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, start_birate, max_birate, Now());

    // Configure ALR for periodic probing.
    probe_ctrl_->set_enable_periodic_alr_probing(true);
    probe_ctrl_->set_alr_start_time(Now());

    auto estimated_bitrate = max_birate / 10;
    probes = probe_ctrl_->OnEstimatedBitrate(estimated_bitrate, Now());
    EXPECT_TRUE(probes.empty());

    // Set a max allocated bitrate below the current estimate.
    auto max_allocated_bitrate = estimated_bitrate - kMultiplier;
    probes = probe_ctrl_->OnMaxTotalAllocatedBitrate(max_allocated_bitrate, Now());
    // No probe since lower than current max.
    EXPECT_TRUE(probes.empty());  

    // Probes such as ALR capped at 2x the max allocation limit.
    clock_.AdvanceTimeMs(5000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, 2 * max_allocated_bitrate) << probes[0].target_bitrate.bps();

    // Remove allocation limit.
    EXPECT_TRUE(probe_ctrl_->OnMaxTotalAllocatedBitrate(DataRate::Zero(), Now()).empty());
    clock_.AdvanceTimeMs(5000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, estimated_bitrate * 2) << probes[0].target_bitrate.bps();
}

MY_TEST_F(ProbeControllerTest, ConfigurableProbing) {
    ProbeController::Configuration config;
    config.first_exponential_probe_scale = 2;
    config.second_exponential_probe_scale = 5;
    config.further_exponential_probe_scale = 3;
    config.further_probe_scale = 0.8;
    config.first_allocation_probe_scale = 2;
    config.second_allocation_probe_scale = 0;

    probe_ctrl_.reset(new ProbeController(config));
    const auto max_bitrate = DataRate::BitsPerSec(5000'000);
    auto probes = probe_ctrl_->OnBitrates(kMinBitrate, kStartBitrate, max_bitrate, Now());
    EXPECT_EQ(probes.size(), 2u);
    EXPECT_EQ(probes[0].target_bitrate, kStartBitrate * 2);
    EXPECT_EQ(probes[1].target_bitrate, kStartBitrate * 5);

    // Repeated probe should only be sent when estimated bitrate climbs above
    // 0.8 * 5 * kStartBitrateBps = 1200.
    auto estimate = DataRate::BitsPerSec(1100);
    probes = probe_ctrl_->OnEstimatedBitrate(estimate, Now());
    EXPECT_EQ(probes.size(), 0u);

    estimate = DataRate::BitsPerSec(1250);
    probes = probe_ctrl_->OnEstimatedBitrate(estimate, Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, 3 * estimate);

    clock_.AdvanceTimeMs(5000);
    probes = probe_ctrl_->OnPeriodicProcess(Now());

    probe_ctrl_->set_alr_start_time(Now());
    auto max_total_allocated_bitrate = DataRate::BitsPerSec(200'000);
    probes = probe_ctrl_->OnMaxTotalAllocatedBitrate(max_total_allocated_bitrate, Now());
    EXPECT_EQ(probes.size(), 1u);
    EXPECT_EQ(probes[0].target_bitrate, max_total_allocated_bitrate * 2);
}
    
} // namespace test
} // namespace naivertc
